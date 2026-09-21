"""File24 rope rotation: full native accumulator, receipts and codec parity."""
import unittest
import test_world_spike as s

w = s.w


def rope(index=0, angle=0, paused=0):
    row = [0]*w.WORDS
    row[:3] = [index,0x1aa,2]
    row[7] = angle&1023
    row[12:14] = [512,1]
    row[18:21] = [77,(angle+32768)%65536-32768,10]
    row[26:30] = [1000,1000,1000,1]
    row[38] = paused
    return row


class RopeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        s.SpikeCodecTests.setUpClass.__func__(cls)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_native_angle_wrap_and_codec_parity(self):
        for angle in (0,1023,1024,32767,32768,65530,65535):
            row = rope(angle=angle)
            self.assertTrue(w.row_valid(row))
            self.assertTrue(self.lib.anchor_world_row_valid(self.Row(*row)))
            self.assertEqual(row[19]&65535, angle)
            self.assertEqual(w.controller_progress(row), 0)
            for column in range(w.WORDS):
                for value in (-2147483648,-32769,-32768,-1,0,1,2,10,75,76,77,512,
                              999,1000,1023,1024,32767,32768,65535,2147483647):
                    altered = list(row); altered[column] = value
                    self.assertEqual(w.row_valid(altered), bool(
                        self.lib.anchor_world_row_valid(self.Row(*altered))),
                        (angle,column,value))

    def test_constructor_and_malformed_fields_rejected(self):
        for column,value in ((3,1),(7,1),(10,1),(11,1),(12,12),(13,0),(13,3),(14,1),
                             (17,1),(18,76),(20,11),(21,1),(28,1200),(29,17),
                             (34,1),(38,2),(40,0x08000064),(47,1)):
            row = rope();row[column] = value
            self.assertFalse(w.row_valid(row), (column,value))
            self.assertFalse(self.lib.anchor_world_row_valid(self.Row(*row)))

    def test_late_entry_pause_handoff_and_rotation_cadence(self):
        a,b = w.WorldTransport(),w.WorldTransport()
        ca,cb = s.context(1),s.context(2)
        ca['room'] = cb['room'] = 0x41
        live = rope(angle=65530,paused=1)
        s.tick(a,ca,[],0)
        _,packets = s.tick(b,cb,[live],0);b.sent(packets,True,0)
        for ctx,other,tx in ((ca,cb,b),(cb,ca,a)):
            ctx['players'][other['cid']] = dict(online=True,isSaveLoaded=True,
                teamId='default',roomId=0x41,interactionSession=other['session'],
                worldSync=tx.advertisement(other))
        _,packets = s.tick(b,cb,[live],1)
        for packet in packets:self.assertTrue(a.receive(ca,packet,1.001))
        b.sent(packets,True,1)
        fresh = rope();fresh[w.INSTANCE] = 81
        result,packets = s.tick(a,ca,[fresh],1.02)
        self.assertEqual(a.owners[0],2)
        self.assertTrue(all(not p['a'] for p in packets))
        applied = list(result['a'][0][w.DELIVERY_ROW:]);applied[38] = 0
        self.assertTrue(applied[w.RECEIPT]&w.BOOTSTRAP)
        self.assertEqual(applied[19],-6)
        _,packets = s.tick(a,ca,[applied],1.12)
        self.assertEqual(a.owners[0],1)
        self.assertEqual(packets[0]['a'][0][19],-6)
        a.sent(packets,True,1.12)
        advanced = list(applied);advanced[19] = advanced[7] = 4
        self.assertFalse(s.tick(a,ca,[advanced],1.18)[1])
        self.assertTrue(s.tick(a,ca,[advanced],1.33)[1])


if __name__ == '__main__':
    unittest.main()
