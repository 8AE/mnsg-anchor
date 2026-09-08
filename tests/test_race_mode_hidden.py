"""Startup-menu visibility contract for the in-development Race mode."""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STARTUP_MENU = ROOT / "src" / "ui" / "startup_menu.c"


class RaceModeVisibilityTests(unittest.TestCase):
    def test_race_menu_entry_and_dispatch_are_compiled_out(self) -> None:
        source = STARTUP_MENU.read_text()
        self.assertIn("#define RACE_MODE_ENABLED 0", source)

        disabled_source = re.sub(
            r"#if RACE_MODE_ENABLED\n.*?#endif\n?", "", source, flags=re.DOTALL
        )
        self.assertNotIn("on_race_clicked", disabled_source)
        self.assertNotIn('make_mode_button(s_menu_ctx, body, "Race"', disabled_source)
        self.assertNotIn("anchor_startup_multiplayer_open_for_race", disabled_source)

    def test_race_implementation_is_preserved_for_later(self) -> None:
        runtime_header = (ROOT / "include" / "anchor_runtime.h").read_text()
        multiplayer_ui = (
            ROOT / "src" / "ui" / "startup_multiplayer_ui.c"
        ).read_text()

        self.assertIn("anchor_startup_multiplayer_open_for_race", runtime_header)
        self.assertIn("anchor_startup_multiplayer_open_for_race", multiplayer_ui)


if __name__ == "__main__":
    unittest.main()
