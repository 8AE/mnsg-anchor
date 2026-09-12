"""Framed Impact-battle coordination tests without a game or live server."""

import json
import unittest
from unittest import mock

from test_boss_invitation_transport import load_client

import anchor_impact


def state(encounter=1, stage=0x0220, fill=1):
    words = [fill] * anchor_impact.ROOT_WORDS
    words[0] = 2000  # boss HP
    words[1] = 100   # ammo
    words[2] = 999   # mech HP
    return {"k": encounter, "s": stage, "r": words}


class ImpactStateSchemaTests(unittest.TestCase):
    def test_update_client_state_includes_impact_metadata(self):
        client = load_client(cid=2, session=101, team="blue", room=0x1D1)
        self.addCleanup(client.disconnect)
        client.set_local_room(0x1D1)
        client._sock.sent.clear()
        self.assertTrue(client.update_client_state("{}"))
        payloads = [json.loads(raw[:-1]) for raw in client._sock.sent]
        state = next(
            p.get("state", {})
            for p in payloads
            if p.get("type") == "UPDATE_CLIENT_STATE"
        )
        self.assertIn(anchor_impact.METADATA_KEY, state)

    def test_hit_authority_accepts_peer_without_matching_room(self):
        # The boss-rush stage is not reported through room metadata, so the
        # hit-authority gate must not reject peers on room mismatch.
        transport = anchor_impact.ImpactTransport()
        transport.set_stage(0x0260)
        transport.local = (True, 1, False)
        adv = transport.advertisement({"session": 202})
        ctx = {
            "cid": 2, "session": 101, "team": "t", "connected": True,
            "loaded": True, "room": 0x01D1,
            "players": {
                3: {
                    "online": True, "isSaveLoaded": False, "teamId": "t",
                    "roomId": 0x01D1, "interactionSession": 202,
                    "playerEpoch": 7,
                    anchor_impact.METADATA_KEY: adv,
                },
            },
        }
        self.assertIsNotNone(transport._peer(ctx, 3, True))

    def test_exact_schema(self):
        value = state()
        self.assertIs(anchor_impact.validate_state(value), value)
        self.assertEqual(len(value["r"]), anchor_impact.ROOT_WORDS)
        self.assertLessEqual(
            len(json.dumps(value, separators=(",", ":")).encode()),
            anchor_impact.MAX_STATE_BYTES,
        )

    def test_rejects_malformed(self):
        # The title-menu boss rush uses stage 0x0260.
        self.assertIsNotNone(
            anchor_impact.validate_state(state(stage=0x0260)))
        bad = [
            None,
            [],
            {},
            {**state(), "extra": 1},
            {**state(), "r": [0] * (anchor_impact.ROOT_WORDS - 1)},
            {**state(), "r": [0] * (anchor_impact.ROOT_WORDS + 1)},
            {**state(), "r": [True] + [0] * (anchor_impact.ROOT_WORDS - 1)},
            {**state(), "r": [0, 0]},
            {**state(), "k": 0},
            {**state(), "k": 5},
            {**state(), "s": 0x0016},
            {**state(), "s": 0x10000},
        ]
        for value in bad:
            with self.subTest(value=value):
                self.assertIsNone(anchor_impact.validate_state(value))

    def test_metadata_version_and_stage_room(self):
        advertisement = [anchor_impact.VERSION, 1, 1, 0, 1, 101,
                         0, 0, 0, 0, 0, 0, 0, 1, 0x220, 1]
        self.assertEqual(anchor_impact.metadata(advertisement), advertisement)
        transport = anchor_impact.ImpactTransport()
        self.assertEqual(transport.room, anchor_impact.ROOM)
        transport.set_stage(0x0223)
        self.assertEqual(transport.room, 0x0223)
        transport.set_stage(anchor_impact.IMPACT_BOSS_RUSH_STAGE)
        self.assertEqual(transport.room, anchor_impact.IMPACT_BOSS_RUSH_STAGE)
        transport.set_stage(0x1234)
        self.assertEqual(transport.room, anchor_impact.IMPACT_BOSS_RUSH_STAGE)


class ImpactElectionTests(unittest.TestCase):
    """Two clients on one Impact stage elect one publisher of the shared mech."""

    ROOM = 0x0220

    def setUp(self):
        self.now = 100.0
        self.clock = mock.patch("time.monotonic", side_effect=lambda: self.now)
        self.clock.start()
        self.addCleanup(self.clock.stop)
        self.a = anchor_impact.ImpactTransport()
        self.b = anchor_impact.ImpactTransport()
        self.a.set_stage(self.ROOM)
        self.b.set_stage(self.ROOM)
        self.history = []
        self.contexts = {}

    def _seed(self, transport, cid, session):
        return {"cid": cid, "session": session, "team": "t",
                "connected": True, "loaded": True, "room": self.ROOM,
                "players": {cid: {"playerEpoch": 1}}}

    def _advertisement(self, transport, cid, session):
        return transport.advertisement(self._seed(transport, cid, session))

    # The boss-rush stage is not reported through the ordinary room metadata,
    # so simulate contexts whose roomId is some unrelated room.
    ORDINARY_ROOM = 0x01D1

    def _context(self, transport, cid, session, peer_cid, peer_session,
                 peer_adv):
        players = {
            cid: {"playerEpoch": 1, "interactionSession": session},
            peer_cid: {
                "online": True, "isSaveLoaded": True, "teamId": "t",
                "roomId": self.ORDINARY_ROOM,
                "interactionSession": peer_session,
                "playerEpoch": 1, anchor_impact.METADATA_KEY: peer_adv,
            }
        }
        return {"cid": cid, "session": session, "team": "t",
                "connected": True, "loaded": True, "room": self.ORDINARY_ROOM,
                "players": players}

    def _tick(self, advance, state_json):
        self.now += advance
        adv_a = self._advertisement(self.a, 2, 101)
        adv_b = self._advertisement(self.b, 3, 202)
        ctx_a = self._context(self.a, 2, 101, 3, 202, adv_b)
        ctx_b = self._context(self.b, 3, 202, 2, 101, adv_a)
        self.contexts = {"a": ctx_a, "b": ctx_b}
        status_a, out_a = self.a.update(ctx_a, 1, 1, 0, state_json, self.now)
        status_b, out_b = self.b.update(ctx_b, 1, 1, 0, state_json, self.now)
        for packet in out_a:
            self.b.receive(ctx_b, packet, self.now)
            self.history.append(packet)
        for packet in out_b:
            self.a.receive(ctx_a, packet, self.now)
            self.history.append(packet)
        return status_a, status_b

    def test_election_and_follower_adoption(self):
        status_a = status_b = None
        for _ in range(6):
            status_a, status_b = self._tick(0.1, state())
        self.assertEqual(sorted([status_a["role"], status_b["role"]]), [1, 2])
        self.assertEqual(status_a["state"], state())
        self.assertEqual(status_b["state"], state())

    def test_cross_stage_packets_are_not_adopted(self):
        for _ in range(6):
            self._tick(0.1, state())
        # A packet from a different Impact stage must not reach this room.
        self.b.set_stage(0x0221)
        adv_a = self._advertisement(self.a, 2, 101)
        ctx_b = self._context(self.b, 3, 202, 2, 101, adv_a)
        packet = {
            "type": anchor_impact.PACKET_TYPE, "v": anchor_impact.VERSION,
            "clientId": 2, "targetTeamId": "t", "session": 101,
            "op": "s", "e": [2, 101, 1], "term": 1,
            "visit": 1, "q": 1, "p": 0, "d": state(), "a": [],
            "s": self.ROOM, "k": 1,
        }
        self.assertFalse(self.b.receive(ctx_b, packet, self.now))

    def test_wire_packets_are_versioned_and_frame_fitted(self):
        for _ in range(6):
            self._tick(0.1, state())
        self.assertTrue(self.history)
        for packet in self.history:
            self.assertEqual(packet["type"], anchor_impact.PACKET_TYPE)
            self.assertEqual(packet["v"], anchor_impact.VERSION)
            self.assertLessEqual(
                len(json.dumps(packet, separators=(",", ":")).encode()) + 1,
                8 * 1024,
            )


if __name__ == "__main__":
    unittest.main()
