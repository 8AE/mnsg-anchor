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
        self.assertEqual(len(commands), 33)
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
        self.assertEqual(vertex[1], 0x09001800)
        self.assertIn((0xFC129A25, 0xFF37FFFF), commands)
        self.assertIn((0xBA000C02, 0x00000000), commands)
        self.assertIn("#define NAMEPLATE_MODEL 0x490018c0u", SOURCE)
        self.assertIn("sizeof(NameplateBankData) == 0x1a38", SOURCE)

    def test_health_tile_fits_tmem_and_keeps_its_native_color(self):
        words = display_words()
        commands = list(zip(words[::2], words[1::2]))
        image = [c for c in commands if c[0] >> 24 == 0xFD][1]
        block = [c for c in commands if c[0] >> 24 == 0xF3][1]
        render_tile = [c for c in commands if c[0] >> 24 == 0xF5][3]
        tile_size = [c for c in commands if c[0] >> 24 == 0xF2][1]
        self.assertEqual(image, (0xFD100000, 0x09001000))
        self.assertEqual((image[0] >> 21) & 7, 0)  # RGBA
        self.assertEqual((image[0] >> 19) & 3, 2)  # RGBA16
        width, height, bytes_per_texel = 64, 16, 2
        words_per_row = (width * bytes_per_texel + 7) // 8
        # libultra CALC_DXT: round 2^11 up to a whole-word row step.
        expected_dxt = ((1 << 11) + words_per_row - 1) // words_per_row
        self.assertEqual((((block[1] >> 12) & 0xFFF) + 1) * 2,
                         width * height * bytes_per_texel)
        self.assertEqual(block[1] & 0xFFF, expected_dxt)
        self.assertEqual((render_tile[0] >> 9) & 0x1FF, words_per_row)
        self.assertEqual(((tile_size[1] >> 12) & 0xFFF) // 4 + 1, 64)
        self.assertEqual((tile_size[1] & 0xFFF) // 4 + 1, 16)
        suffix = commands.index(image)
        white = commands.index((0xFB000000, 0xFFFFFFFF))
        self.assertGreater(white, suffix)
        self.assertLess(white, commands.index((0x0400103F, 0x09001880)))
        self.assertIn("s_heart_loaded ? s_heart_rgba : 0", SOURCE)
        self.assertIn("slot->health != health", SOURCE)
        self.assertIn("data->display[38] = health_width ?", SOURCE)

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
