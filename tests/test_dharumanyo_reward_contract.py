"""Dharumanyo reward synchronization contracts; no game or server required."""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ITEM_SYNC = (ROOT / "src" / "item_sync.c").read_text()
BOSS_SYNC = (ROOT / "src" / "boss_sync.c").read_text()
ANCHOR_CLIENT = (ROOT / "py" / "anchor_mnsg.py").read_text()


def function_body(source: str, name: str) -> str:
    """Return a C/Python source region bounded by balanced C braces."""
    match = re.search(rf"\b{name}\s*\([^;]*?\)\s*\{{", source, re.DOTALL)
    if not match:
        raise AssertionError(f"function {name} was not found")

    depth = 1
    cursor = match.end()
    while cursor < len(source) and depth:
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
        cursor += 1
    if depth:
        raise AssertionError(f"function {name} has unbalanced braces")
    return source[match.start():cursor]


class DharumanyoRewardContractTests(unittest.TestCase):
    def test_flower_and_completion_use_the_verified_durable_keys(self) -> None:
        self.assertRegex(
            ITEM_SYNC,
            r'\{\s*0x258\s*,\s*0\s*,\s*0\s*,\s*"mi_flower"\s*\}',
        )
        self.assertRegex(
            ITEM_SYNC,
            r'\{\s*0x018\s*,\s*0\s*,\s*"fl_dharmanyo"\s*\}',
        )

        # Scenario 0x73 belongs to a different reward. Keep Star and Flower
        # distinct so a Dharumanyo completion cannot grant the wrong item.
        self.assertRegex(
            ITEM_SYNC,
            r'\{\s*0x250\s*,\s*0\s*,\s*0\s*,\s*"mi_star"\s*\}',
        )

    def test_native_reward_story_flags_remain_durable(self) -> None:
        expected = {
            "cs_dhrm_1": 0x073,
            "cs_dhrm_2": 0x070,
            "cs_dhrm_3": 0x071,
            "cs_dhrm_4": 0x072,
        }
        for name, flag_id in expected.items():
            with self.subTest(name=name):
                self.assertRegex(
                    ITEM_SYNC,
                    rf'\{{\s*0x{flag_id:03X}\s*,\s*0\s*,\s*"{name}"\s*\}}',
                )

    def test_snapshots_apply_and_serialize_fields_before_flags(self) -> None:
        for function_name in ("apply_team_state", "build_team_state_json"):
            with self.subTest(function=function_name):
                body = function_body(ITEM_SYNC, function_name)
                self.assertLess(body.index("s_fields[i]"), body.index("s_flag_bits[i]"))

    def test_incremental_sync_uses_queued_set_flag_before_boss_flags(self) -> None:
        body = function_body(ITEM_SYNC, "monitor_and_send_changes")
        fields = body.index("/* 32-bit save-data fields. */")
        flags = body.index("/* Single-bit flags. */")
        queued_send = body.index("boss_sync_send_local_progress(")

        self.assertLess(fields, queued_send)
        self.assertLess(queued_send, flags)

    def test_remote_native_reward_changes_use_the_bounded_send_gate(self) -> None:
        body = function_body(ITEM_SYNC, "monitor_and_send_changes")
        self.assertEqual(body.count("boss_sync_send_local_progress("), 2)
        self.assertIn("BOSS_SYNC_PROGRESS_SUPPRESSED", body)

        commit = function_body(ITEM_SYNC, "item_sync_commit_boss_completion")
        self.assertIn("cache_darumanyo_native_reward()", commit)

    def test_flower_needs_no_hot_packet_or_loose_collectible_cleanup(self) -> None:
        hot_budget = ANCHOR_CLIENT[
            ANCHOR_CLIENT.index("HOT_PACKET_MAX_BYTES"):
            ANCHOR_CLIENT.index("HOT_PACKET_MAX_BYTES") + 800
        ]
        self.assertNotIn("FLOWER", hot_budget)

        visual_filter = function_body(ITEM_SYNC, "is_visual_collectible_field")
        self.assertIn('"mr_ely_"', visual_filter)
        self.assertIn('"mr_arr_"', visual_filter)
        self.assertNotIn("mi_flower", visual_filter)
        self.assertNotIn('"mi_"', visual_filter)

    def test_combat_teardown_keeps_remote_completion_deferred(self) -> None:
        body = function_body(
            BOSS_SYNC, "boss_sync_finish_darumanyo_native_death"
        )

        self.assertIn("s_darumanyo_state.victory_complete = 1", body)
        self.assertNotIn("item_sync_commit_boss_completion", body)
        self.assertNotIn("remote_defeat_in_progress = 0", body)

    def test_reward_controller_releases_only_after_native_completion_flag(self) -> None:
        entry = function_body(
            BOSS_SYNC, "boss_sync_observe_darumanyo_reward_controller"
        )
        returned = function_body(
            BOSS_SYNC, "boss_sync_finish_darumanyo_reward_controller"
        )

        self.assertIn(
            '#define ENTITY_DARUMANYO_REWARD_CONTROLLER 0x034Fu',
            BOSS_SYNC,
        )
        self.assertIn(
            'RECOMP_HOOK("func_08000090_72F920")',
            BOSS_SYNC,
        )
        self.assertIn(
            'RECOMP_HOOK_RETURN("func_08000090_72F920")',
            BOSS_SYNC,
        )
        self.assertIn("D_800C7AB2 == DARUMANYO_ROOM", entry)
        self.assertIn(
            "ACTOR_ENTITY_ID(actor) == ENTITY_DARUMANYO_REWARD_CONTROLLER",
            entry,
        )
        self.assertIn("s_darumanyo_state.victory_complete", returned)
        self.assertIn("s_darumanyo_state.local_defeat_started", returned)
        native_flag = returned.index(
            "func_800240DC_24CDC(DARUMANYO_KILL_FLAG)"
        )
        commit = returned.index(
            'item_sync_commit_boss_completion("fl_dharmanyo")'
        )
        self.assertLess(native_flag, commit)

        # The observer acknowledges durable state only. The original callback
        # must retain ownership of states 25-28, child cleanup and control.
        self.assertNotIn("func_80034EF8_35AF8", returned)
        self.assertNotIn("ACTOR_STATUS", returned)
        self.assertNotIn("ACTOR_ENTITY_ID", returned)


if __name__ == "__main__":
    unittest.main()
