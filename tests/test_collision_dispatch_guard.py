import struct
import unittest

from tools.check_collision_dispatch import SYMBOL, check_dispatch


def elf_fixture(words, *, name=SYMBOL):
    """Small layout-faithful ELF with an unrelated indirect-dispatch function."""
    names = b"\0" + name.encode() + b"\0unrelated\0"
    instructions = struct.pack(">" + "I" * (len(words) + 1), *words, 0x00200008)
    code_offset = 52
    strings_offset = code_offset + len(instructions)
    symbols_offset = strings_offset + len(names)
    symbols = (bytes(16) + struct.pack(">IIIBBH", 1, 0x81001000, len(words) * 4, 2, 0, 1) +
               struct.pack(">IIIBBH", len(name) + 2, 0x81001000 + len(words) * 4, 4, 2, 0, 1))
    sections_offset = symbols_offset + len(symbols)
    ident = b"\x7fELF\x01\x02\x01" + bytes(9)
    header = struct.pack(">HHIIIIIHHHHHH", 2, 8, 1, 0, 0, sections_offset, 0,
                         52, 0, 0, 40, 4, 0)
    sections = [bytes(40),
                struct.pack(">10I", 0, 1, 6, 0x81001000, code_offset, len(instructions), 0, 0, 4, 0),
                struct.pack(">10I", 0, 3, 0, 0, strings_offset, len(names), 0, 0, 1, 0),
                struct.pack(">10I", 0, 2, 0, 0, symbols_offset, len(symbols), 2, 1, 4, 16)]
    return ident + header + instructions + names + symbols + b"".join(sections)


class CollisionDispatchGuardTests(unittest.TestCase):
    def test_return_and_direct_call_pass_without_scanning_other_functions(self):
        check_dispatch(elf_fixture([0x0C000000, 0, 0x03E00008, 0]))

    def test_interior_jump_table_dispatch_is_rejected(self):
        with self.assertRaisesRegex(ValueError, r"0x81001004: jr \$1"):
            check_dispatch(elf_fixture([0, 0x00200008]))

    def test_indirect_call_is_rejected(self):
        with self.assertRaisesRegex(ValueError, r"jalr \$25"):
            check_dispatch(elf_fixture([0x0320F809]))

    def test_missing_function_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "found 0"):
            check_dispatch(elf_fixture([0x03E00008], name="other"))

    def test_invalid_and_truncated_files_fail_cleanly(self):
        for contents in (b"", bytes(52), elf_fixture([0x03E00008])[:-1]):
            with self.subTest(length=len(contents)), self.assertRaises(ValueError):
                check_dispatch(contents)


if __name__ == "__main__":
    unittest.main()
