import re
import unittest
from pathlib import Path


SOURCE = (Path(__file__).resolve().parents[1] / "src" / "ui" /
          "anchor_nameplates.c").read_text()


def display_words():
    block = SOURCE.split("static const unsigned int display[", 1)[1]
    block = block.split("= {", 1)[1].split("};", 1)[0]
    return [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]+)u", block)]


class NativeNameplateContractTests(unittest.TestCase):
    def test_ia8_strip_fits_tmem_and_quad_uv(self):
        words = display_words()
        commands = list(zip(words[::2], words[1::2]))
        self.assertEqual(len(commands), 20)
        load_image = next(c for c in commands if c[0] >> 24 == 0xFD)
        load_block = next(c for c in commands if c[0] >> 24 == 0xF3)
        set_tile = [c for c in commands if c[0] >> 24 == 0xF5]
        tile_size = next(c for c in commands if c[0] >> 24 == 0xF2)
        vertex = next(c for c in commands if c[0] >> 24 == 0x04)

        self.assertEqual(load_image[1], 0x0B000000)
        self.assertEqual((load_image[0] >> 21) & 7, 3)  # IA
        self.assertEqual((load_image[0] >> 19) & 3, 2)  # 16b load path
        texels = ((load_block[1] >> 12) & 0xFFF) + 1
        self.assertEqual(texels * 2, 4096)
        self.assertEqual(load_block[1] & 0xFFF, 64)  # DXT for 32 words/row
        self.assertEqual((set_tile[1][0] >> 21) & 7, 3)  # IA render tile
        self.assertEqual((set_tile[1][0] >> 19) & 3, 1)  # IA8
        self.assertEqual((set_tile[1][0] >> 9) & 0x1FF, 32)
        self.assertEqual(((tile_size[1] >> 12) & 0xFFF) // 4 + 1, 256)
        self.assertEqual((tile_size[1] & 0xFFF) // 4 + 1, 16)
        self.assertEqual(vertex[1], 0x09001000)
        self.assertIn((0xFC129A25, 0xFF37FFFF), commands)
        self.assertIn((0xBA000C02, 0x00000000), commands)
        self.assertIn("#define NAMEPLATE_MODEL 0x49001040u", SOURCE)
        self.assertIn("sizeof(NameplateBankData) == 0x1150", SOURCE)

    def test_nameplate_is_native_and_bank_owned(self):
        self.assertNotIn("recompui_", SOURCE)
        self.assertIn('RECOMP_HOOK_RETURN("func_80016950_17550")', SOURCE)
        self.assertIn('RECOMP_HOOK("func_80016C44_17844")', SOURCE)
        self.assertIn("D_80167FC0_168BC0[i].data =", SOURCE)
        self.assertIn("s_banks[i * NAMEPLATE_BANKS + bank]", SOURCE)

    def test_file_binder_cannot_replace_private_segment_bases(self):
        self.assertIn("NAMEPLATE_SCALE, 0, 0);", SOURCE)
        self.assertIn("*(unsigned short *)(object + 0x3c) = 0;", SOURCE)
        self.assertIn("*(unsigned short *)(object + 0x4c) = 0;", SOURCE)
        self.assertIn("*(unsigned int *)(object + 0x40)", SOURCE)
        self.assertIn("*(unsigned int *)(object + 0x50)", SOURCE)


if __name__ == "__main__":
    unittest.main()
