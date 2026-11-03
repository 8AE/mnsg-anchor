"""Exercise victim-confirmed PvP healing through the production transport."""
import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
import test_player_interaction_transport as interaction

anchor = interaction.anchor_mnsg


class PlayerLifeStealTransportTests(unittest.TestCase):
    setUp = interaction.PlayerInteractionTransportTests.setUp
    tearDown = interaction.PlayerInteractionTransportTests.tearDown
    send_position = staticmethod(interaction.PlayerInteractionTransportTests.send_position)
    movement = staticmethod(interaction.PlayerInteractionTransportTests.movement)
    hit = staticmethod(interaction.PlayerInteractionTransportTests.hit)

    def request(self, **changes):
        arguments = dict(damage=1, life_steal=1)
        arguments.update(changes)
        self.assertTrue(anchor.send_player_hit(2, 7, 1, 2, 3, **arguments))
        return self.sock.sent[-1]

    @staticmethod
    def result(hit, loss=1, **changes):
        packet = {key: hit[key] for key in
                  ("roomId", "sourceSession", "targetSession", "sourceEpoch", "targetEpoch", "hitSeq")}
        packet.update(type="MNSG_PLAYER_HIT_RESULT", clientId=hit["targetClientId"],
                      targetClientId=hit["clientId"], healthLoss=loss, quiet=True)
        packet.update(changes)
        return packet

    def test_result_is_direct_correlated_and_consumed_once(self):
        hit = self.request()
        self.assertEqual(hit["lifeSteal"], 1)
        self.assertIsNone(anchor.poll_player_hit_heal(3))
        result = self.result(hit, 2)  # native Sudden Impact doubles loss
        self.assertTrue(anchor._receive_player_hit_result(result))
        self.assertFalse(anchor._receive_player_hit_result(result))
        self.assertEqual(anchor.poll_player_hit_heal(3), (3, 2))
        self.assertIsNone(anchor.poll_player_hit_heal(3))
        for packet in (hit, result):
            self.assertNotIn("targetTeamId", packet)
            self.assertNotIn("addToQueue", packet)
            self.assertLessEqual(len(json.dumps(packet, separators=(",", ":")).encode()) + 1, 512)
        self.assertFalse(anchor.has_packet())

    def test_ordinary_hits_never_authorize_healing(self):
        hit = self.request(life_steal=0)
        self.assertNotIn("lifeSteal", hit)
        self.assertFalse(anchor._receive_player_hit_result(self.result(hit)))
        self.assertFalse(anchor._player_hit_pending)
        for invalid in (True, "1", -1, 2, 1.0):
            self.assertFalse(anchor.send_player_hit(2, 7, 1, 2, 3, life_steal=invalid))
            self.assertFalse(anchor._receive_player_hit(self.hit(lifeSteal=invalid)))
        self.assertFalse(anchor.send_player_hit(2, 7, 1, 2, 3, hit_kind=1, life_steal=1))

    def test_malformed_unsolicited_or_mismatched_results_cannot_consume_pending(self):
        hit = self.request()
        result = self.result(hit)
        changes = [{"healthLoss": value} for value in (0, -1, 3, 256, True, "1", 1.0)]
        changes += [{field: result[field] + 1} for field in
                    ("clientId", "targetClientId", "roomId", "sourceSession", "targetSession",
                     "sourceEpoch", "targetEpoch", "hitSeq")]
        changes += [{"type": "MNSG_PLAYER_HIT"}]
        for field in result:
            if field != "quiet":
                omitted = dict(result)
                del omitted[field]
                self.assertFalse(anchor._receive_player_hit_result(omitted))
        for field in ("clientId", "targetClientId", "roomId", "sourceSession", "targetSession",
                      "sourceEpoch", "targetEpoch", "hitSeq", "healthLoss"):
            for value in (None, True, "1", 1.0):
                self.assertFalse(anchor._receive_player_hit_result({**result, field: value}))
        for change in changes:
            with self.subTest(change=change):
                self.assertFalse(anchor._receive_player_hit_result({**result, **change}))
        self.assertTrue(anchor._receive_player_hit_result(result))
        self.assertEqual(anchor.poll_player_hit_heal(3), (3, 1))

    def test_receipts_report_actual_loss_once_and_zero_loss_is_silent(self):
        self.assertTrue(anchor._receive_player_hit(self.hit(lifeSteal=1)))
        polled = anchor.poll_player_hit(with_receipt=True)
        self.assertGreater(polled[-1], 0)
        self.assertFalse(anchor.finish_player_hit(polled[-1], 0))
        self.assertFalse(anchor.finish_player_hit(polled[-1], 1))
        self.assertFalse(self.sock.sent)
        legacy = self.hit(hitSeq=2, lifeSteal=1)
        del legacy["damage"]
        self.assertTrue(anchor._receive_player_hit(legacy))
        receipt = anchor.poll_player_hit(with_receipt=True)[-1]
        self.assertTrue(anchor.finish_player_hit(receipt, 1))
        result = self.sock.sent[-1]
        self.assertEqual(result["clientId"], 1)
        self.assertEqual(result["targetClientId"], 2)
        self.assertEqual(result["healthLoss"], 1)
        self.assertFalse(anchor.finish_player_hit(receipt, 1))

    def test_pending_receipts_and_heals_expire_and_reset(self):
        hit = self.request()
        self.clock.return_value = 101.001
        self.assertFalse(anchor._receive_player_hit_result(self.result(hit)))
        self.assertFalse(anchor._player_hit_pending)
        self.clock.return_value = 100.0
        self.assertTrue(anchor._receive_player_hit(self.hit(lifeSteal=1)))
        receipt = anchor.poll_player_hit(with_receipt=True)[-1]
        self.clock.return_value = 100.501
        self.assertFalse(anchor.finish_player_hit(receipt, 1))
        self.clock.return_value = 100.0
        hit = self.request()
        self.assertTrue(anchor._receive_player_hit_result(self.result(hit)))
        self.clock.return_value = 101.001
        self.assertIsNone(anchor.poll_player_hit_heal(3))
        self.clock.return_value = 100.0
        self.request()
        anchor.disconnect()
        self.assertFalse(anchor._player_hit_pending)
        self.assertFalse(anchor._player_hit_receipts)
        self.assertFalse(anchor._player_hit_heals)

    def test_attacker_lifetime_and_room_changes_discard_healing(self):
        hit = self.request()
        self.assertTrue(anchor._receive_player_hit_result(self.result(hit)))
        self.assertIsNone(anchor.poll_player_hit_heal(4))  # C epoch is newer than last POS
        hit = self.request()
        self.assertTrue(anchor._receive_player_hit_result(self.result(hit)))
        self.assertTrue(self.send_position(epoch=4))
        self.assertFalse(anchor._player_hit_heals)
        self.request()
        anchor.set_local_room(11)
        anchor.set_local_room(10)
        self.assertFalse(anchor._player_hit_pending)

    def test_confirmed_lethal_hit_survives_victim_death_pose(self):
        hit = self.request()
        self.assertTrue(anchor._receive_player_hit_result(self.result(hit)))
        anchor._player_states[2].update(playerEpoch=8, collisionDisabled=1)
        self.assertEqual(anchor.poll_player_hit_heal(3), (3, 1))
        # A victim death POS may also overtake the receipt in the relay.
        anchor._player_states[2].update(playerEpoch=7, collisionDisabled=0)
        hit = self.request()
        anchor._player_states[2].update(playerEpoch=8, collisionDisabled=1)
        self.assertTrue(anchor._receive_player_hit_result(self.result(hit)))
        self.assertEqual(anchor.poll_player_hit_heal(3), (3, 1))

    def test_failed_send_rolls_back_and_fast_result_is_not_lost(self):
        with mock.patch.object(anchor, "_send_raw", return_value=False):
            self.assertFalse(anchor.send_player_hit(2, 7, 1, 2, 3, life_steal=1))
        self.assertFalse(anchor._player_hit_pending)
        self.assertEqual(anchor._player_hit_seq, 0)
        def reply_during_send(packet):
            self.assertTrue(anchor._receive_player_hit_result(self.result(packet)))
            return True
        with mock.patch.object(anchor, "_send_raw", side_effect=reply_during_send):
            self.assertTrue(anchor.send_player_hit(2, 7, 1, 2, 3, life_steal=1))
        self.assertEqual(anchor.poll_player_hit_heal(3), (3, 1))
        def reply_before_failed_send(packet):
            self.assertTrue(anchor._receive_player_hit_result(self.result(packet)))
            return False
        with mock.patch.object(anchor, "_send_raw", side_effect=reply_before_failed_send):
            self.assertFalse(anchor.send_player_hit(2, 7, 1, 2, 3, life_steal=1))
        self.assertIsNone(anchor.poll_player_hit_heal(3))

    def test_storage_bounds_and_expired_pending_can_be_reused(self):
        for cid in range(2, anchor.PLAYER_HIT_RECEIPT_COUNT + 3):
            anchor._player_states[cid] = copy.deepcopy(anchor._player_states[2])
            self.assertTrue(anchor.send_player_hit(cid, 7, 1, 2, 3, life_steal=1))
        self.assertEqual(len(anchor._player_hit_pending), 64)
        self.assertEqual(len(self.sock.sent), 65)
        self.assertEqual(self.sock.sent[-1]["targetClientId"], 66)
        self.assertNotIn("lifeSteal", self.sock.sent[-1])
        self.assertEqual(set(anchor._player_hit_pending), set(range(1, 65)))
        self.assertFalse(anchor._receive_player_hit_result(self.result(self.sock.sent[-1])))
        self.assertTrue(anchor._receive_player_hit_result(self.result(self.sock.sent[0])))
        self.assertEqual(anchor.poll_player_hit_heal(3), (3, 1))
        self.clock.return_value = 101.001
        self.request()
        self.assertEqual(len(anchor._player_hit_pending), 1)

    def test_peer_session_and_room_changes_reject_results(self):
        hit = self.request()
        for change in ({"interactionSession": 203}, {"roomId": 11}, {"online": False}):
            peer = copy.deepcopy(anchor._player_states[2])
            anchor._player_states[2].update(change)
            self.assertFalse(anchor._receive_player_hit_result(self.result(hit)))
            anchor._player_states[2] = peer
        self.assertTrue(anchor._receive_player_hit_result(self.result(hit)))

    def test_lifesteal_packet_worst_case_values_fit_budgets(self):
        hit = self.request(damage=8)
        worst = dict(hit)
        for field in ("clientId", "targetClientId", "sourceSession", "targetSession",
                      "sourceEpoch", "targetEpoch", "hitSeq", "sourcePosSeq"):
            worst[field] = anchor._POSITION_SEQUENCE_MASK
        worst.update(roomId=0xffff, hitT=9999999999999999,
                     hitX=-10000000.0, hitY=-10000000.0, hitZ=-10000000.0)
        for packet in (worst, self.result(worst, loss=16)):
            self.assertLessEqual(len(json.dumps(packet, separators=(",", ":")).encode()) + 1, 512)

    def test_two_peers_exchange_hit_and_result_through_receive_dispatch(self):
        spec = importlib.util.spec_from_file_location("anchor_lifesteal_peer", ROOT / "py/anchor_mnsg.py")
        victim = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(victim)
        self.addCleanup(victim.disconnect)
        victim._connected = True
        victim._client_id = 2
        victim._local_room_id = 10
        victim._interaction_session = 202
        victim._player_states = copy.deepcopy(anchor._player_states)
        victim._player_states[1]["online"] = True
        victim._sock = interaction.RecordingSocket()
        hit = self.request()
        raw_hit = (json.dumps(hit) + "\x00").encode()
        incoming = interaction.RecordingSocket((raw_hit, b""))
        victim._sock = incoming
        with mock.patch.object(victim, "_do_disconnect"):
            victim._recv_loop(incoming)
        polled = victim.poll_player_hit(with_receipt=True)
        self.assertEqual(polled[:2], (1, 7))
        self.assertTrue(victim.finish_player_hit(polled[-1], 1))
        result = incoming.sent[-1]
        receiver = interaction.RecordingSocket(((json.dumps(result) + "\x00").encode(), b""))
        anchor._sock = receiver
        with mock.patch.object(anchor, "_do_disconnect"):
            anchor._recv_loop(receiver)
        self.assertFalse(anchor.has_packet())
        self.assertFalse(victim.has_packet())
        self.assertEqual(anchor.poll_player_hit_heal(3), (3, 1))
        self.assertIsNone(anchor.poll_player_hit_heal(3))


if __name__ == "__main__":
    unittest.main()
