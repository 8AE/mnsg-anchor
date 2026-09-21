"""File24 spike checkpoints: production codec parity and transport lifecycle."""
import copy
import ctypes
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'py'))
import anchor_world as w


def spike(index=0, subtype=2, phase=1, timer=90, frame=0, paused=0):
    row = [0]*w.WORDS
    row[:3] = [index, 0x3ca, 2]
    row[10:14] = [2 if subtype == 1 else 0, frame, 12, 0]
    row[17:24] = [timer if phase == 1 else -1, 76, subtype, phase,
                  (24,23,20)[subtype], 90 if subtype else 155, int(phase == 4)]
    row[26:30] = [1000,1000,1000 if subtype else 1200, 1 if phase == 1 else 17]
    row[38] = paused
    return row


def context(cid):
    return dict(cid=cid, session=cid*10, team='default', connected=True,
                loaded=True, room=0x32, players={})


def tick(transport, ctx, rows, now):
    return transport.update(ctx, ctx['room'], 42, 1, rows, '00'*32, now)


class SpikeTransportTests(unittest.TestCase):
    def setUp(self):
        self.a, self.b = w.WorldTransport(), w.WorldTransport()
        self.ca, self.cb = context(1), context(2)
        tick(self.a, self.ca, [], 0)
        # The higher-ID incumbent has already completed local initialization.
        _, packets = tick(self.b, self.cb, [spike(0), spike(1)], 0)
        self.b.sent(packets, True, 0)
        self.assertIn(0, self.b.established)
        for ctx, other, transport in ((self.ca,self.cb,self.b), (self.cb,self.ca,self.a)):
            ctx['players'][other['cid']] = dict(online=True, isSaveLoaded=True,
                teamId=other['team'], roomId=other['room'],
                interactionSession=other['session'], worldSync=transport.advertisement(other))

    def send(self, rows, now=1):
        _, packets = tick(self.b, self.cb, rows, now)
        self.assertTrue(packets)
        for packet in packets:
            self.assertTrue(self.a.receive(self.ca, packet, now+.001))
        self.b.sent(packets, True, now)
        return packets

    def test_late_lower_id_must_apply_before_competing(self):
        live = spike(phase=4, frame=150, paused=1)
        self.send([live])
        fresh = spike(timer=120); fresh[w.INSTANCE] = 77
        result, packets = tick(self.a, self.ca, [fresh], 1.02)
        self.assertEqual(self.a.owners[0], 2)
        self.assertNotIn(0, self.a.established)
        self.assertTrue(all(not p['a'] for p in packets))
        offer = result['a'][0]
        self.assertEqual(offer[0], 2)
        self.assertEqual(offer[w.DELIVERY_ROW+w.INSTANCE], 77)
        self.assertTrue(offer[w.DELIVERY_ROW+w.RECEIPT] & w.BOOTSTRAP)
        self.assertEqual(offer[w.DELIVERY_ROW+11], 150)
        self.assertEqual(offer[w.DELIVERY_ROW+17], -1)
        self.assertEqual(offer[w.DELIVERY_ROW+20], 4)
        self.assertEqual(offer[w.DELIVERY_ROW+23], 1)
        # No native receipt means no takeover, even on repeated updates.
        tick(self.a, self.ca, [fresh], 1.04)
        self.assertEqual(self.a.owners[0], 2)
        applied = list(offer[w.DELIVERY_ROW:]); applied[38] = 0
        _, published = tick(self.a, self.ca, [applied], 1.12)
        self.assertIn(0, self.a.established)
        self.assertEqual(self.a.owners[0], 1)
        self.assertTrue(published)
        wire = published[0]['a'][0]
        self.assertEqual(wire[:38], live[:38])
        self.assertEqual(wire[w.INSTANCE:w.RECEIPT+1], [0,0])
        # Leaving the room cannot rewind the acknowledged reverse stroke.
        self.ca['players'][2]['online'] = False
        _, after = tick(self.a, self.ca, [applied], 1.4)
        self.assertTrue(after)
        self.assertEqual(after[0]['a'][0][17:24], live[17:24])

    def test_distinct_spike_indices_keep_their_own_stagger(self):
        self.send([spike(0, timer=10), spike(1, timer=70)])
        fresh = [spike(0), spike(1)]
        for row in fresh: row[w.INSTANCE] = 9 + row[0]
        result, _ = tick(self.a, self.ca, fresh, 1.02)
        self.assertEqual({r[w.DELIVERY_ROW]: r[w.DELIVERY_ROW+17]
                          for r in result['a']}, {0:10,1:70})
        self.assertEqual({r[w.DELIVERY_ROW+w.INSTANCE] for r in result['a']}, {9,10})

    def test_room_team_session_version_and_replay_rejected(self):
        packet = self.send([spike()])[0]
        self.assertFalse(self.a.receive(self.ca, packet, 1.02))
        for column, value in ((0,w.VERSION-1),(1,999),(2,999),(3,0x34),(4,99)):
            bad = copy.deepcopy(packet); bad['q'] += 1; bad['m'][column] = value
            self.assertFalse(self.a.receive(self.ca, bad, 1.03), column)
        bad = copy.deepcopy(packet); bad['q'] += 1; bad['targetTeamId'] = 'isolated'
        self.assertFalse(self.a.receive(self.ca, bad, 1.04))
        bad = copy.deepcopy(packet); bad['q'] += 1; bad['a'][0][18] = 75
        self.assertFalse(self.a.receive(self.ca, bad, 1.05))

    def test_phase_edges_and_steady_timer_and_frame_cadence(self):
        transport, ctx = w.WorldTransport(), context(1)
        row = spike(timer=40)
        _, packets = tick(transport, ctx, [row], 1)
        self.assertTrue(packets); transport.sent(packets, True, 1)
        row[17] = 39
        self.assertFalse(tick(transport, ctx, [row], 1.06)[1])
        _, packets = tick(transport, ctx, [row], 1.21)
        self.assertTrue(packets); transport.sent(packets, True, 1.21)
        extended = spike(phase=3, frame=5)
        self.assertFalse(tick(transport, ctx, [extended], 1.24)[1])
        _, packets = tick(transport, ctx, [extended], 1.27)
        self.assertTrue(packets); transport.sent(packets, True, 1.27)
        extended[11] = 10
        self.assertFalse(tick(transport, ctx, [extended], 1.33)[1])
        _, packets = tick(transport, ctx, [extended], 1.48)
        self.assertTrue(packets); transport.sent(packets, True, 1.48)
        reversed_row = spike(phase=4, frame=200)
        _, packets = tick(transport, ctx, [reversed_row], 1.54)
        self.assertTrue(packets)
        self.assertEqual(packets[0]['a'][0][20], 4)

    def test_bounded_team_packets_and_failed_send_retry(self):
        transport, ctx = w.WorldTransport(), context(1)
        rows = [spike(i, subtype=i%3, phase=4, frame=299) for i in range(256)]
        _, packets = tick(transport, ctx, rows, 1)
        self.assertEqual(sum(len(p['a']) for p in packets), 256)
        self.assertLessEqual(len(packets), w.MAX_PARTS)
        for packet in packets:
            self.assertEqual(packet['targetTeamId'], 'default')
            self.assertEqual(packet['clientId'], 1)
            self.assertTrue(packet['quiet'])
            self.assertNotIn('addToQueue', packet)
            self.assertNotIn('targetClientId', packet)
            self.assertLessEqual(len(json.dumps(packet, separators=(',',':')).encode())+1,
                                 w.PACKET_BYTES)
        transport.sent(packets, False, 1)
        self.assertFalse(tick(transport, ctx, rows, 1.02)[1])
        _, retry = tick(transport, ctx, rows, 1.06)
        self.assertTrue(retry)
        self.assertEqual(retry[0]['q'], packets[0]['q']+1)


class SpikeCodecTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        lib = Path(cls.temp.name)/'spike.dylib'
        subprocess.run(['cc','-shared','-fPIC','-std=c99','-Wall','-Wextra','-Werror',
                        '-Wno-misleading-indentation','-I'+str(ROOT/'include'),
                        str(ROOT/'src/utils/anchor_world_codec.c'),
                        str(ROOT/'src/utils/string_utils.c'),'-o',str(lib)], check=True)
        cls.lib = ctypes.CDLL(str(lib))
        cls.Row = ctypes.c_int*w.WORDS
        cls.lib.anchor_world_row_valid.argtypes = [ctypes.POINTER(ctypes.c_int)]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_all_subtype_phases_and_native_codec_parity(self):
        values = (-2147483648,-3276801,-32769,-1,0,1,2,3,4,12,17,20,23,24,
                  25,75,76,90,155,156,200,299,300,1000,1200,3276701,2147483647)
        for subtype in range(3):
            for phase in (1,3,4):
                row = spike(subtype=subtype, phase=phase, frame=150, paused=1)
                self.assertTrue(w.row_valid(row))
                self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*row)))
                self.assertEqual(w.controller_progress(row), 0)
                for column in range(w.WORDS):
                    for value in values:
                        altered = list(row); altered[column] = value
                        self.assertEqual(w.row_valid(altered), bool(
                            self.lib.anchor_world_row_valid(self.Row(*altered))),
                            (subtype,phase,column,value))

    def test_forbidden_native_states_and_pointer_fields(self):
        invalid = {3:1,10:2,11:300,12:13,13:1,14:1,17:-1,18:75,19:3,
                   20:0,21:21,22:91,23:1,24:1,25:1,26:999,27:999,28:1200,
                   29:17,30:1,31:1,32:1,33:1,34:1,35:1,36:1,37:1,38:2,39:1}
        for column, value in invalid.items():
            row = spike(); row[column] = value
            self.assertFalse(w.row_valid(row), (column,value))
            self.assertFalse(self.lib.anchor_world_row_valid(self.Row(*row)))
        for column in range(40,48):
            row = spike(); row[column] = 0x08000050
            self.assertFalse(w.row_valid(row))
            self.assertFalse(self.lib.anchor_world_row_valid(self.Row(*row)))
        for column in (0,19,20,38):
            row = spike(); row[column] = True
            self.assertFalse(w.row_valid(row))


if __name__ == '__main__':
    unittest.main()
