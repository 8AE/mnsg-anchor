"""Native scenario83 and crane-camera boundaries of the shared save tables."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CraneProgressTests(unittest.TestCase):
    def test_capabilities_and_power_share_without_local_camera_flags(self):
        source = (ROOT / 'src/progression/item_sync.c').read_text()
        fields = re.search(r'static SyncField s_fields\[\] = \{(.*?)\n\};', source, re.S)[1]
        flags = re.search(r's_flag_bits\[\] = \{(.*?)\n\};', source, re.S)[1]
        field_map = {int(offset, 16): key for offset, key in re.findall(
            r'\{(0x[0-9A-Fa-f]+),\s*0,\s*[01],\s*"([^"]+)"\}', fields)}
        flag_map = {int(flag, 16): key for flag, key in re.findall(
            r'\{(0x[0-9A-Fa-f]+),\s*0,\s*"([^"]+)"\}', flags)}
        # Scenario83 writes both 32-bit capabilities; consumption is a separate bit.
        self.assertEqual(field_map[0xC8], 'eq_camera')
        self.assertEqual(field_map[0xD8], 'cam_charge')
        self.assertEqual(flag_map[0x1A3], 'pk_camera')
        self.assertEqual(flag_map[0x15A], 'cr_power')
        # Native room occupancy continually sets/clears these camera handshakes.
        # They must be absent from both outbound snapshots and inbound key lookup.
        self.assertNotIn(0x15B, flag_map)
        self.assertNotIn(0x15C, flag_map)
        self.assertNotIn('cr_off_txt', flag_map.values())
        self.assertNotIn('cr_entered', flag_map.values())
