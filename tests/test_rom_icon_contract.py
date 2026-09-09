import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class RomIconContractTests(unittest.TestCase):
    def test_no_baked_hud_icon_assets_remain(self):
        for name in ("flute", "goemon", "ebisumaru", "sasuke", "yae"):
            self.assertFalse((ROOT / "icons" / f"{name}_icon.png").exists())
            self.assertFalse((ROOT / "icons" / f"{name}_icon.rgba").exists())
            self.assertFalse((ROOT / "include" / f"icon_{name}.h").exists())

        self.assertFalse((ROOT / "tools" / "gen_icon_headers.py").exists())

    def test_build_does_not_extract_or_generate_hud_icons(self):
        build_script = (ROOT / "build_mod.sh").read_text()
        self.assertNotIn("gen_icon_headers", build_script)
        self.assertNotIn("extract_flute_icon", build_script)

    def test_runtime_loader_pins_verified_rom_resources(self):
        header = (ROOT / "include" / "anchor_rom_icons.h").read_text()
        ui_source = (ROOT / "src" / "ui" / "anchor_ui.c").read_text()
        nameplate_source = (ROOT / "src" / "anchor_nameplates.c").read_text()

        self.assertIn("ANCHOR_FLUTE_RESOURCE_ID          0x8016u", header)
        self.assertIn(
            "ANCHOR_FLUTE_RESOURCE_ROM_ADDRESS 0x007EB740u", header
        )
        self.assertIn("ANCHOR_MAP_FACE_RESOURCE_ID          0x868Cu", header)
        self.assertIn(
            "ANCHOR_MAP_FACE_RESOURCE_ROM_ADDRESS 0x013F08D0u", header
        )
        self.assertIn("anchor_rom_load_flute_icon_rgba32", ui_source)
        self.assertIn("anchor_rom_load_map_face_icons_rgba32", ui_source)
        self.assertIn("anchor_rom_load_map_face_icons_rgba32", nameplate_source)
        self.assertNotIn('#include "icon_', ui_source)
        self.assertNotIn('#include "icon_', nameplate_source)


if __name__ == "__main__":
    unittest.main()
