"""Framed Control Machine transport and late-intro observer behavior."""

import copy
import json
import unittest

import anchor_control_machine as cm
from test_boss_invitation_transport import load_client
from test_dharumanyo_transport import Relay as BossRelay


ROOT = [0] * cm.ROOT_WORDS
ROOT[0:3] = [2, 11, 5]
ROOT[15:18] = [0x3e4ccccd] * 3  # native child scale 0.2
ROOT[19:22] = [0, 5, 256]  # initial status, HP mirror, no flash
ROOT[22] = 0xff000000
ROOT[23] = 1  # first authoritative command
SAMPLE = {"fight": cm.VERSION, "r": ROOT, "p": []}


def native_checkpoint(phase, status, hp, mirror, timer, serial=1):
    """Use the observed constructor values, then vary native fight edges."""
    state = copy.deepcopy(SAMPLE)
    state["r"][0:3] = [phase, timer, hp]
    state["r"][15:18] = [0x3e4ccccd] * 3  # child scale 0.2
    state["r"][18:21] = [0, status, mirror]  # frame, global status, HP mirror
    state["r"][23] = serial
    return state


class Relay(BossRelay):
    def add(self, cid, room=cm.ROOM, team="blue", session=None):
        client = load_client(cid, session or cid * 101, team, room)
        self.test.addCleanup(client.disconnect)
        self.clients[cid] = client
        self.states[cid] = copy.deepcopy(SAMPLE)
        self.ready[cid] = room == cm.ROOM
        self.visit[cid] = 1
        self.paused[cid] = 0
        client._player_states[cid]["playerEpoch"] = 7
        client._player_states[cid]["roomId"] = room
        self.roster()
        return client

    def roster(self):
        for receiver in self.clients.values():
            members = []
            for cid, client in self.clients.items():
                members.append({
                    "clientId": cid, "self": receiver is client,
                    "name": client._player_name, "online": client._connected,
                    "teamId": client._team_id,
                    "isSaveLoaded": client._local_save_loaded,
                    "currentRoomId": client._local_room_id,
                    "interactionSession": client._interaction_session,
                    cm.METADATA_KEY: client._control_machine.advertisement(
                        client._boss_context()
                    ),
                })
            receiver._replace_all_client_states(members)
            for cid in self.clients:
                receiver._player_states[cid]["playerEpoch"] = 7

    def tick(self, advance=0.0, flush=True):
        self.now += advance
        for cid, client in self.clients.items():
            if client._connected:
                self.status[cid] = json.loads(client.update_control_machine(
                    self.ready[cid], self.visit[cid], self.paused[cid],
                    json.dumps(self.states[cid]) if self.states[cid] is not None
                    else "null",
                ))
        if flush:
            self.flush()
        return self.status

    def packets(self, op=None):
        return [(stamp, cid, packet) for stamp, cid, packet in self.history
                if packet["type"] == cm.PACKET_TYPE and
                (op is None or packet["op"] == op)]


class ControlMachineTransportTests(unittest.TestCase):
    def setUp(self):
        self.net = Relay(self)
        self.owner = self.net.add(2)
        self.net.tick()
        self.net.tick(0.36)
        self.assertEqual(self.net.status[2]["role"], 1)

    def observer(self, cid=1, team="blue"):
        guest = self.net.add(cid, team=team)
        self.net.ready[cid] = 0
        self.net.states[cid] = None
        self.net.roster()
        self.net.tick(0.01)
        self.net.tick(0.11)
        self.net.tick(0.01)
        return guest

    def test_exact_native_checkpoint_shape_and_size(self):
        largest = copy.deepcopy(SAMPLE)
        largest["r"] = [0xffffffff] * cm.ROOT_WORDS
        largest["r"][0] = 9
        largest["r"][21] = 256
        largest["p"] = [[0xffffffff] * cm.PROJECTILE_WORDS
                        for _ in range(cm.MAX_PROJECTILES)]
        self.assertIs(cm.validate_state(largest), largest)
        self.assertLessEqual(len(json.dumps(largest, separators=(",", ":")).encode()),
                             cm.MAX_STATE_BYTES)
        packet = self.owner._control_machine._packet(
            self.owner._boss_context(), "s", q=0x7fffffff, p=0,
            d=largest, a=[[0x7fffffff] * 4] * self.owner._control_machine.ack_rows,
        )
        self.assertLessEqual(len(json.dumps(packet, separators=(",", ":")).encode()) + 1,
                             8 * 1024)
        hp_zero = copy.deepcopy(SAMPLE)
        hp_zero["r"][2] = 0
        self.assertIs(cm.validate_state(hp_zero), hp_zero)
        old_version = copy.deepcopy(SAMPLE)
        old_version["fight"] = 1
        old_version["r"] = old_version["r"][:-1]
        self.assertIsNone(cm.validate_state(old_version))
        self.assertIsNone(cm.metadata([1] +
            self.owner._control_machine.advertisement(
                self.owner._boss_context())[1:]))
        bad = [None, [], {}, {**SAMPLE, "extra": 1},
               {**SAMPLE, "fight": 0}, {**SAMPLE, "fight": True},
               {**SAMPLE, "fight": 1},
               {**SAMPLE, "r": [0] * (cm.ROOT_WORDS - 1)},
               {**SAMPLE, "r": [0] * (cm.ROOT_WORDS + 1)},
               {**SAMPLE, "r": [True] + SAMPLE["r"][1:]},
               {**SAMPLE, "r": [0] + SAMPLE["r"][1:]},
               {**SAMPLE, "r": SAMPLE["r"][:21] + [257] + SAMPLE["r"][22:]},
               {**SAMPLE, "r": SAMPLE["r"][:23] + [0]},
               {**SAMPLE, "r": SAMPLE["r"][:23] + [True]},
               {**SAMPLE, "r": SAMPLE["r"][:23] + [0x100000000]},
               {**SAMPLE, "p": [[0] * (cm.PROJECTILE_WORDS - 1)]},
               {**SAMPLE, "p": [[0] * cm.PROJECTILE_WORDS] *
                (cm.MAX_PROJECTILES + 1)},
               {**SAMPLE, "p": [[-1] + [0] * (cm.PROJECTILE_WORDS - 1)]}]
        for value in bad:
            with self.subTest(value=value):
                self.assertIsNone(cm.validate_state(value))

    def test_old_wire_version_is_rejected_before_checkpoint_adoption(self):
        guest = self.net.add(3)
        self.net.start()
        before = guest._control_machine.state
        packet = copy.deepcopy(self.net.packets("s")[-1][2])
        packet["v"] = 1
        packet["q"] += 1
        self.assertFalse(guest._control_machine.receive(
            guest._boss_context(), packet, self.net.now))
        self.assertEqual(guest._control_machine.state, before)

    def test_late_observer_previews_live_fight_without_ownership_or_hits(self):
        guest = self.observer()
        status = self.net.status[1]
        self.assertEqual(status["role"], 0)
        self.assertEqual(status["encounter"], [0, 0, 0])
        self.assertEqual(status["owner"], 2)
        self.assertEqual(status["preview"], SAMPLE)
        self.assertFalse(guest.send_control_machine_hit(1))
        advertisement = guest._control_machine.advertisement(
            guest._boss_context()
        )
        self.assertEqual(advertisement[1:4], [1, 1, 1])
        self.assertEqual(advertisement[12:14], [0, 0])
        self.assertEqual({cid for _, cid, p in self.net.packets("s")}, {2})
        self.assertTrue(all(p["targetTeamId"] == "blue" and
                            p.get("quiet") and "addToQueue" not in p
                            for _, _, p in self.net.packets()))
        self.net.ready[1] = 1
        self.net.states[1] = copy.deepcopy(SAMPLE)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[1]["role"], 2)
        self.assertEqual(self.net.status[1]["state"], SAMPLE)
        self.assertEqual(self.net.status[1]["owner"], 2)

    def test_observer_owner_hint_waits_for_fresh_checkpoint(self):
        guest = self.net.add(1)
        self.net.ready[1] = 0
        self.net.states[1] = None
        self.net.roster()
        self.net.tick(0.01, flush=False)
        status = self.net.status[1]
        self.assertEqual(status["role"], 0)
        self.assertEqual(status["encounter"], [0, 0, 0])
        self.assertEqual(status["owner"], 2)
        self.assertIsNone(status["preview"])
        self.assertIsNone(guest._control_machine.state)
        self.assertFalse(guest.send_control_machine_hit(1))
        self.net.flush()
        requests = [(cid, packet) for _, cid, packet in self.net.packets("r")]
        self.assertEqual(len(requests), 1)
        self.assertEqual(requests[0][0], 1)
        self.assertEqual(requests[0][1]["visit"], 1)
        self.assertEqual(requests[0][1]["targetClientId"], 2)
        self.assertIsNotNone(self.owner._control_machine._peer(
            self.owner._boss_context(), 1))
        self.assertTrue(self.owner._control_machine.force_snapshot)
        self.net.tick(0.11)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[1]["owner"], 2)
        self.assertEqual(self.net.status[1]["preview"], SAMPLE)

    def test_preview_invalidates_on_visit_room_team_and_owner_departure(self):
        guest = self.observer()
        self.net.visit[1] = 2
        self.net.tick(0.01)
        self.assertIsNone(self.net.status[1]["preview"])
        # A same-visit owner checkpoint may be requested again after arrival.
        self.net.tick(1.01)
        self.net.tick(0.11)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[1]["preview"], SAMPLE)
        guest._team_id = "red"
        self.net.tick(0.01)
        self.assertIsNone(self.net.status[1]["preview"])
        self.assertEqual(self.net.status[1]["owner"], 0)
        guest._team_id = "blue"
        self.net.tick(0.01)
        self.assertIsNone(self.net.status[1]["preview"])
        guest.set_local_room(0x154)
        self.net.tick(0.01)
        self.assertIsNone(self.net.status[1]["preview"])
        self.assertEqual(self.net.status[1]["owner"], 0)
        guest.set_local_room(cm.ROOM)
        self.net.visit[1] = 3
        self.net.roster()
        self.net.tick(1.01)
        self.net.tick(0.11)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[1]["preview"], SAMPLE)
        self.owner._connected = False
        self.net.roster()
        self.net.tick(0.01)
        self.assertIsNone(self.net.status[1]["preview"])
        self.assertEqual(self.net.status[1]["role"], 0)
        self.assertEqual(self.net.status[1]["owner"], 0)

    def test_fight_packets_are_bounded_and_cross_team_isolated(self):
        self.observer()
        stranger = self.observer(3, "red")
        self.assertIsNone(self.net.status[3]["preview"])
        self.assertIsNone(stranger._control_machine.state)
        self.net.history.clear()
        for frame in range(60):
            self.net.states[2] = copy.deepcopy(SAMPLE)
            self.net.states[2]["r"][1] = frame + 12
            self.net.tick(1 / 60)
        packets = self.net.packets("s")
        self.assertGreater(len(packets), 0)
        self.assertLessEqual(len(packets), 10)
        self.assertTrue(all(b[0] - a[0] >= 0.1 - 1e-8
                            for a, b in zip(packets, packets[1:])))
        self.assertTrue(all(len(json.dumps(p, separators=(",", ":")).encode()) + 1
                            <= 8 * 1024 for _, _, p in packets))
        self.assertIsNone(self.net.status[3]["preview"])

    def test_same_command_serial_still_updates_projectile_state(self):
        self.net.add(3)
        self.net.start()
        before = self.net.status[3]["revision"]
        shot = copy.deepcopy(SAMPLE)
        shot["r"][1] += 1
        shot["p"] = [[1, 10, 80, 0x3f800000, 0, 0, 0, 0, 0]]
        self.assertEqual(shot["r"][23], SAMPLE["r"][23])
        self.net.states[2] = shot
        self.net.tick(0.11)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[3]["state"], shot)
        self.assertGreater(self.net.status[3]["revision"], before)

    def test_remote_hit_is_direct_one_damage_and_retried_until_ack(self):
        attacker = self.net.add(3)
        self.net.start()
        self.assertEqual(self.net.status[3]["role"], 2)
        self.assertTrue(attacker.send_control_machine_hit(1))
        self.net.tick(0.11)
        hits = self.net.packets("h")
        self.assertEqual(len(hits), 1)
        self.assertEqual(hits[0][2]["targetClientId"], 2)
        self.assertEqual(hits[0][2]["h"], [7, 1, 1])
        self.net.tick(0.11)
        self.assertEqual(self.net.status[2]["hits"], [[3, 303, 7, 1, 1]])
        self.net.tick(0.11)
        self.assertFalse(attacker._control_machine.outgoing)
        self.assertEqual(self.net.status[2]["hits"], [])
        self.assertTrue(attacker.send_control_machine_hit(1))
        self.assertFalse(attacker._control_machine.send_hit(
            attacker._boss_context(), 2, 2, self.net.now))
        bad = copy.deepcopy(hits[0][2])
        bad["h"][0] = 6  # stale player epoch
        self.assertFalse(self.owner._control_machine.receive(
            self.owner._boss_context(), bad, self.net.now))

    def test_owner_native_hit_publishes_hp_without_self_hit_packet(self):
        self.net.add(3)
        self.net.start()
        self.net.history.clear()
        # A native contact can briefly prevent a capture. Once C supplies
        # the next native state, transport must publish it without a self-hit.
        self.net.states[2] = None
        self.net.tick(0.11)
        self.assertEqual(self.net.status[3]["state"], SAMPLE)
        next_state = copy.deepcopy(SAMPLE)
        next_state["r"][0] = 5
        next_state["r"][2] = 4
        next_state["r"][3] = 0x50
        next_state["r"][19:21] = [1, 4]
        next_state["r"][23] = 2
        self.net.states[2] = next_state
        self.net.tick(0.11)
        self.net.tick(0.01)
        self.assertEqual(self.net.status[2]["state"], next_state)
        self.assertEqual(self.net.status[3]["state"], next_state)
        self.assertTrue(any(cid == 2 and p["d"] == next_state
                            for _, cid, p in self.net.packets("s")))
        self.assertEqual(self.net.packets("h"), [])

    def test_rejected_remote_hit_ack_keeps_hp_then_next_hit_updates(self):
        attacker = self.net.add(3)
        self.net.start()
        self.net.history.clear()
        self.assertTrue(attacker.send_control_machine_hit(1))
        self.net.tick(0.11)
        self.net.tick(0.11)
        self.assertEqual(self.net.status[2]["hits"], [[3, 303, 7, 1, 1]])
        # The native owner rejected a hit during invulnerability. Publishing
        # an unchanged valid state still acknowledges the delivered intent.
        self.net.tick(0.11)
        self.assertFalse(attacker._control_machine.outgoing)
        self.assertEqual(self.net.status[2]["state"]["r"][2], 5)
        self.assertEqual(self.net.status[3]["state"]["r"][2], 5)
        self.assertEqual(len(self.net.packets("h")), 1)
        self.assertTrue(any(p.get("a") == [[3, 303, 7, 1]]
                            for _, cid, p in self.net.packets("s")
                            if cid == 2))

        self.assertTrue(attacker.send_control_machine_hit(2))
        self.net.tick(0.11)
        self.net.tick(0.11)
        self.assertEqual(self.net.status[2]["hits"], [[3, 303, 7, 2, 1]])
        next_state = copy.deepcopy(SAMPLE)
        next_state["r"][0] = 5
        next_state["r"][2] = 4
        next_state["r"][3] = 0x50
        next_state["r"][19:21] = [1, 4]
        next_state["r"][23] = 2
        self.net.states[2] = next_state
        self.net.tick(0.11)
        self.net.tick(0.01)
        self.assertFalse(attacker._control_machine.outgoing)
        self.assertEqual(self.net.status[3]["state"]["r"][2], 4)

    def test_fresh_higher_term_owner_beats_stale_lower_cid_preview(self):
        guest = self.observer()
        successor = self.net.add(3)
        self.net.start()
        self.assertEqual(self.net.status[3]["owner"], 2)
        # Keep the old owner online in the roster, but stop its updates long
        # enough for the replica to take over with a newer term.
        self.net.now += 3.2
        status = json.loads(successor.update_control_machine(
            1, 1, 0, json.dumps(SAMPLE)))
        self.assertEqual((status["role"], status["owner"], status["term"]),
                         (1, 3, 2))
        self.assertEqual(status["state"]["r"][23], 1)
        self.net.flush()
        preview = json.loads(guest.update_control_machine(0, 1, 1, "null"))
        self.assertEqual(preview["preview"], SAMPLE)
        self.assertEqual(preview["preview"]["r"][23], 1)
        self.assertEqual(guest._control_machine.cache[(2, 202, 1)]["owner"], 3)
        self.assertTrue(self.owner._player_states[2]["online"])

    def test_late_lower_id_preserves_owner_and_reconnect_preserves_successor(self):
        late = self.net.add(1)
        self.net.start()
        self.assertEqual((self.net.status[1]["role"],
                          self.net.status[1]["owner"]), (2, 2))
        self.owner._connected = False
        self.net.roster()
        self.net.tick(0.11)
        self.assertEqual((self.net.status[1]["role"],
                          self.net.status[1]["owner"],
                          self.net.status[1]["term"]), (1, 1, 2))
        reconnected = self.net.add(2, session=404)
        self.net.visit[2] = 2
        self.net.roster()
        self.net.tick(0.11)
        self.net.tick(0.11)
        self.assertEqual(self.net.status[2]["role"], 2)
        self.assertEqual(self.net.status[2]["owner"], 1)
        self.assertEqual(self.net.status[1]["owner"], 1)
        self.assertIsNot(reconnected, self.owner)
        self.assertIsNot(late, reconnected)

    def test_passive_preview_expires_without_owner_refresh(self):
        guest = self.observer()
        self.net.now += guest._control_machine.lease + 0.01
        status = json.loads(guest.update_control_machine(0, 1, 1, "null"))
        self.assertIsNone(status["preview"])
        self.assertEqual(status["role"], 0)
        self.assertFalse(guest.send_control_machine_hit(1))

    def test_unload_and_new_session_retire_passive_observer(self):
        guest = self.observer()
        self.assertEqual(self.net.status[1]["preview"], SAMPLE)
        guest.set_save_loaded(False)
        self.assertEqual(guest._control_machine.observing_visit, 0)
        self.assertIsNone(guest._control_machine.observer_scope)
        self.assertEqual(guest._control_machine.advertisement(
            guest._boss_context())[1:3], [0, 0])
        guest.set_save_loaded(True)
        self.net.tick(0.01)
        self.assertIsNone(self.net.status[1]["preview"])
        guest._interaction_session = 1111
        self.net.tick(0.01)
        self.assertIsNone(self.net.status[1]["preview"])
        self.assertEqual(self.net.status[1]["role"], 0)


class ControlMachineNativeStartupTransportTests(unittest.TestCase):
    def test_initial_status_zero_previews_and_followed_phases_advance(self):
        net = Relay(self)
        owner = net.add(2)
        initial = native_checkpoint(2, 0, 5, 5, 11)
        self.assertIs(cm.validate_state(initial), initial)
        net.states[2] = initial
        net.tick()
        net.tick(0.36)
        self.assertEqual(net.status[2]["role"], 1)
        self.assertEqual(net.status[2]["state"], initial)

        guest = net.add(1)
        net.ready[1] = 0
        net.states[1] = None
        net.roster()
        net.tick(0.01)
        net.tick(0.11)
        net.tick(0.01)
        self.assertEqual(net.status[1]["role"], 0)
        self.assertEqual(net.status[1]["owner"], 2)
        self.assertEqual(net.status[1]["preview"], initial)
        self.assertIsNone(guest._control_machine.state)

        net.ready[1] = 1
        net.tick(0.01)
        self.assertEqual(net.status[1]["role"], 2)
        self.assertEqual(net.status[1]["state"], initial)
        revision = net.status[1]["revision"]
        for serial, (phase, status, hp, mirror, timer) in enumerate((
            (3, 0, 5, 5, 12),
            (4, 0, 5, 5, 13),
            (5, 0, 5, 5, 14),
            (5, 1, 4, 4, 15),
            (6, 2, 255, 0, 128),
            (7, 3, 255, 0, 40),
        ), 2):
            checkpoint = native_checkpoint(phase, status, hp, mirror,
                                           timer, serial)
            self.assertIs(cm.validate_state(checkpoint), checkpoint)
            net.states[2] = checkpoint
            net.tick(0.11)
            net.tick(0.01)
            self.assertEqual(net.status[2]["state"], checkpoint)
            self.assertEqual(net.status[1]["state"], checkpoint)
            self.assertEqual(net.status[1]["owner"], 2)
            self.assertEqual(net.status[1]["state"]["r"][23], serial)
            self.assertGreater(net.status[1]["revision"], revision)
            revision = net.status[1]["revision"]
        self.assertEqual({cid for _, cid, _ in net.packets("s")}, {2})
        self.assertTrue(owner._control_machine.local[0])


if __name__ == "__main__":
    unittest.main()
