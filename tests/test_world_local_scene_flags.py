"""Executable regression test for the File67 local scene flags.

Subject: save bits 0x06B (`fl_outerspace`) and 0x06C (`fl_to_space`) are
File67 local travel-choice / scene-clear state.  They were removed from
`src/progression/item_sync.c`'s `s_flag_bits[]` sync table and from the debug catalog in
`src/ui/debug.c`.  Nothing in the item-sync paths may read or write them, while
the neighbouring travel flags (0x017 `fl_kyushu`, 0xC3 `fl_mtfuji`,
0xC4 `fl_shore_entry`) and the four miracle words must keep working.

How it runs: `tests/test_item_scene_flags.c` is a host harness that compiles
the real production `src/progression/item_sync.c` translation unit (with a shimmed
`modding.h`, because the real one emits Mach-O-incompatible section
attributes), links the real `string_utils` / `json_utils` /
`anchor_item_reconcile` sources, and stubs only the Anchor transport, boss,
enemy, miracle and recompui surface.  It then drives the real `apply_flag`,
`apply_incoming_value`, `apply_incoming_flag`, `apply_team_state`,
`build_team_state_json`, `broadcast_team_state_snapshot` and
`monitor_and_send_changes` functions.

Fixture boundary: the Anchor packet drain (`process_incoming_packets`) and the
per-frame hook are not executed; `anchor_has_packet`/`anchor_poll_packet`
return "no packet".  The delta and snapshot application paths are therefore
exercised by calling the production apply entry points directly, which is the
layer this change touches.

Reproducible negative run against the pre-edit source:

    python3 tests/test_world_local_scene_flags.py \
        --source /tmp/mnsg-world-sync/item-sync-before-local-scene-flags.c \
        --expect-fail
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = os.path.join(ROOT, "tests", "test_item_scene_flags.c")
PRODUCTION = os.path.join(ROOT, "src", "progression", "item_sync.c")
REAL_UTILS = [
    os.path.join(ROOT, "src", "utils", "string_utils.c"),
    os.path.join(ROOT, "src", "utils", "json_utils.c"),
    os.path.join(ROOT, "src", "progression", "anchor_item_reconcile.c"),
]

# The pre-edit production source kept by the commander for the negative run.
BASELINE_CANDIDATES = [
    "/tmp/mnsg-world-sync/item-sync-before-local-scene-flags.c",
]
if os.environ.get("MNSG_ITEM_SYNC_BASELINE"):
    BASELINE_CANDIDATES.insert(0, os.environ["MNSG_ITEM_SYNC_BASELINE"])

# The real modding.h carries Mach-O-incompatible section attributes, so the
# host build uses a shim that keeps the imports as plain declarations.
HOST_MODDING_H = """#ifndef MNSG_HOST_MODDING_H
#define MNSG_HOST_MODDING_H
#define RECOMP_IMPORT(mod, func) extern func
#define RECOMP_EXPORT
#define RECOMP_PATCH
#define RECOMP_FORCE_PATCH
#define RECOMP_DECLARE_EVENT(func) void func(void);
#define RECOMP_CALLBACK(mod, event)
#define RECOMP_HOOK(func)
#define RECOMP_HOOK_RETURN(func)
#endif
"""

CC = os.environ.get("HOST_CC", "cc")


def build_host_includes(base):
    """Stage current headers and legacy aliases for the negative fixture."""
    dest = os.path.join(base, "include")
    shutil.copytree(os.path.join(ROOT, "include"), dest)
    for header in Path(dest).rglob("*.h"):
        if header.parent == Path(dest):
            continue
        alias = Path(dest) / header.name
        if not alias.exists():
            alias.write_text('#include "%s"\n' % header.relative_to(dest).as_posix())
    with open(os.path.join(dest, "platform", "modding.h"), "w") as handle:
        handle.write(HOST_MODDING_H)
    reconcile_alias = os.path.join(dest, "utils", "anchor_item_reconcile.h")
    shutil.copy2(os.path.join(ROOT, "src", "progression", "anchor_item_reconcile.h"), reconcile_alias)
    return dest


def compile_harness(base, source):
    """Return (command, binary, include_dir) for the host harness build."""
    include_dir = build_host_includes(base)
    binary = os.path.join(base, "item_scene_flags")
    command = [
        CC,
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-I" + include_dir,
        "-I" + os.path.join(ROOT, "src", "progression"),
        "-I" + ROOT,
        "-DMNSG_ITEM_SYNC_SRC=" + '"%s"' % source,
        HARNESS,
    ] + REAL_UTILS + ["-o", binary]
    return command, binary, include_dir


def build_and_run(source, keep=False):
    """Build the harness against `source` and return (rc, output, command)."""
    base = tempfile.mkdtemp(prefix="mnsg-item-scene-flags-")
    try:
        command, binary, _include_dir = compile_harness(base, source)
        compiled = subprocess.run(command, capture_output=True, text=True)
        if compiled.returncode != 0:
            return (
                compiled.returncode,
                "COMPILE FAILED\n" + compiled.stdout + compiled.stderr,
                command,
            )
        ran = subprocess.run([binary], capture_output=True, text=True)
        return ran.returncode, ran.stdout + ran.stderr, command
    finally:
        if not keep:
            shutil.rmtree(base, ignore_errors=True)


def baseline_source():
    for candidate in BASELINE_CANDIDATES:
        if candidate and os.path.exists(candidate):
            return candidate
    return None


class LocalSceneFlagTests(unittest.TestCase):
    maxDiff = None

    def test_production_source_keeps_scene_bits_local_and_shares_shop_progress(self):
        rc, output, command = build_and_run(PRODUCTION)
        del command
        self.assertEqual(rc, 0, msg="harness failed:\n%s" % output)
        # Every asserted region must actually have run.
        for marker in (
            "1. the two local scene bits are absent",
            "2. a local value of 1 survives",
            "3. compact snapshots cannot set or clear",
            "4. outgoing compact snapshot omits them",
            "5. outgoing monitor publishes nothing",
            "6. the merge snapshot form omits them too",
            "7. legitimate neighbours still apply and publish",
            "8. equipment collection flags apply, persist and publish",
            "9. Cat Eyes quest purchases share through every progression path",
            "10. keys, world equipment and counts remain shared",
        ):
            self.assertIn(marker, output, msg="region did not run: %s" % marker)
        self.assertIn(
            "personal scene bits stay local; shared progression unaffected",
            output,
        )

    @unittest.skipUnless(baseline_source(), "pre-edit baseline unavailable")
    def test_pre_edit_source_fails_the_same_harness(self):
        source = baseline_source()
        rc, output, command = build_and_run(source)
        del command
        self.assertNotEqual(
            rc, 0, msg="the pre-edit table must not satisfy this harness"
        )
        # The failure must be about the two flags, not an unrelated crash.
        self.assertIn("FAIL:", output)
        self.assertTrue(
            "fl_outerspace is not a sync-table key" in output
            or "fl_to_space is not a sync-table key" in output
            or "the compact snapshot JSON omits both local scene bits" in output
            or "monitor_and_send_changes never publishes the local scene bits"
            in output,
            msg="pre-edit failure was not about the local scene flags:\n%s"
            % output,
        )


def main(argv):
    parser = argparse.ArgumentParser(
        description="Run the File67 local-scene-flag regression harness."
    )
    parser.add_argument(
        "--source",
        default=PRODUCTION,
        help="item_sync.c to compile the harness against",
    )
    parser.add_argument(
        "--expect-fail",
        action="store_true",
        help="require the harness to fail (pre-edit reproduction)",
    )
    parser.add_argument(
        "--keep",
        action="store_true",
        help="keep the temporary build directory",
    )
    args, _unknown = parser.parse_known_args(argv)

    source = os.path.abspath(args.source)
    if not os.path.exists(source):
        print("source not found: %s" % source, file=sys.stderr)
        return 2

    rc, output, command = build_and_run(source, keep=args.keep)
    print("source : %s" % source)
    print("compile: %s" % " ".join(command))
    print("--- harness output ---")
    print(output, end="" if output.endswith("\n") else "\n")
    print("--- exit %d ---" % rc)

    if args.expect_fail:
        if rc != 1 or "FAIL:" not in output:
            print("ERROR: expected an assertion failure in the harness", file=sys.stderr)
            return 1
        print("pre-edit reproduction confirmed: the harness fails")
        return 0

    if rc != 0:
        print("ERROR: harness failed", file=sys.stderr)
        return 1
    print("ok")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
