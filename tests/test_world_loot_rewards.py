"""Execute the real item-sync baseline functions with local save-state inputs."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def c_function(source, name):
    match = re.search(rf"void {name}\([^;]*?\)\s*\{{", source, re.S)
    if not match:
        raise AssertionError(f"missing function: {name}")
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


class LootRewardTests(unittest.TestCase):
    def test_private_pickups_preserve_other_same_frame_deltas(self):
        source = (ROOT / "src/progression/item_sync.c").read_text()
        code = """#include <assert.h>
static int s_ds_initialized, s_ryo_initialized, s_ds_prev_hp, s_ryo_prev;
static unsigned int s_ds_prev_char, active_character;
#define DS_CHAR_IDX() active_character
"""
        code += c_function(source, "item_sync_exclude_loot_reward")
        code += c_function(source, "item_sync_exclude_pvp_damage")
        code += """
int main(void) {
    int hp, money;
    s_ds_initialized = s_ryo_initialized = 1;
    s_ds_prev_hp = hp = 10;
    s_ryo_prev = money = 100;
    hp += 2; money += 5;
    item_sync_exclude_loot_reward(2, 5);
    assert(hp - s_ds_prev_hp == 0 && money - s_ryo_prev == 0);

    /* Enemy damage and a purchase remain shareable beside a private pickup. */
    s_ds_prev_hp = hp = 10;
    s_ryo_prev = money = 100;
    hp -= 3; money -= 20;
    hp += 2; money += 5;
    item_sync_exclude_loot_reward(2, 5);
    assert(hp - s_ds_prev_hp == -3 && money - s_ryo_prev == -20);

    /* A separate heal and earning event retain their own deltas. */
    s_ds_prev_hp = hp = 10;
    s_ryo_prev = money = 100;
    hp += 3; money += 20;
    hp += 2; money += 5;
    item_sync_exclude_loot_reward(2, 5);
    assert(hp - s_ds_prev_hp == 3 && money - s_ryo_prev == 20);

    /* PvP exclusion composes with loot without hiding an enemy hit. */
    s_ds_prev_hp = hp = 10;
    hp -= 1; item_sync_exclude_pvp_damage(1);
    hp -= 3; hp += 2;
    item_sync_exclude_loot_reward(2, 0);
    assert(hp - s_ds_prev_hp == -3);

    s_ds_prev_hp = 20; s_ryo_prev = 9999;
    item_sync_exclude_loot_reward(0, 0);
    assert(s_ds_prev_hp == 20 && s_ryo_prev == 9999);
    active_character = 1;
    item_sync_exclude_loot_reward(2, 0);
    assert(s_ds_prev_hp == 20);
    s_ds_initialized = s_ryo_initialized = 0;
    item_sync_exclude_loot_reward(2, 5);
    assert(s_ds_prev_hp == 20 && s_ryo_prev == 9999);
    return 0;
}
"""
        with tempfile.TemporaryDirectory(prefix="mnsg-loot-reward-") as directory:
            path = Path(directory)
            (path / "reward.c").write_text(code)
            result = subprocess.run([os.environ.get("HOST_CC", "cc"), "-std=c99",
                            "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
                            str(path / "reward.c"), "-o", str(path / "reward")],
                           capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(path / "reward")],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
