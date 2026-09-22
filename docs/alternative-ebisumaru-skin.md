# Alternative Ebisumaru skin

The L-button edge toggles the opening cutscene's alternative Ebisumaru appearance on the playable Ebisumaru actor. Appearance bit 3 carries the toggle to remote players. The playable action, animation frame, joint tree, face cues, and gameplay behavior remain native.

## Native trace

The opening director creates a standalone Ebisumaru through `func_80212DA4_67C784`. That creator calls `func_8000DBF0_E7F0` with model `0x68000B0C`, context `0xC006D898`, and file `0x4D9`. The file supplies its own mesh and texture atlas. `func_80212A04_67C3E4` selects only nine opening clips, so its action model cannot replace the playable actor's animation set. The native loader's expanded-size table reserves `0x9D70` bytes for file `0x4D9`: its `0x5D70` raw bytes and four decoded texture pages. See [the opening spawn trace](opening-cutscene-goemon-ebisumaru-spawn.md).

The playable Ebisumaru uses action file `0x127` in object segment 8 (`+0x38`) and broad file `0x124` in segment 9 (`+0x40`). Action records in `D_80203F34_5BFE44[1]` select the playable model and joint tree. Native `func_801DC70C_59861C` stages the active action into a private buffer, subtracts its file offset, and binds that buffer at object `+0x38`. The mesh graft follows this same addressing rule.

The previous implementation copied two texture pages into the clothed broad file, transposing one page. This mixed the clothed mesh UVs and tile layout with the opening actor's different atlas. It produced the corrupted overlay described in the report. The opening file's model points to four texture pages at `+0x5D70`, `+0x6D70`, `+0x7D70`, and `+0x8D70`; its display lists and UVs need to travel with those pages.

## Mesh graft

At stage load, `anchor_player_skin_load_resources()` registers file `0x4D9` through the scene loader and records its resident size. The builder requires at least `0x9D70` bytes, including all four decoded texture pages. It allocates one `0x28000`-byte buffer below `0x80800000` from the scene arena:

| Offset | Contents |
| --- | --- |
| `0x00000..0x17FFF` | Playable broad file `0x124` at its original offsets, including cue/expression data. |
| `0x18000..0x21D6F` | Opening file `0x4D9`, with its mesh, display lists, vertices, and all four texture pages. |

The opening mesh uses segment-8 addresses. The builder rebases the 72 pointer words in its verified display-list region `0x4798..0x5D6F` to segment 9 with the `0x18000` offset. It does not scan vertex or animation bytes, and it leaves decoded texture pixels unchanged. A missing resource, short bound, or unexpected pointer count disables the skin for that stage.

The active playable action is copied into a private slice. `anchor_alternative_patch_action()` walks its joint tree and replaces 14 body display references with the corresponding opening-mesh references. Matching uses the opening and playable tree branches plus the original ROM vertex bounds. Extra gameplay nodes, such as action props, retain their playable displays. The opening tree has one additional node without a playable counterpart. The playable model header, joint links, motion samples, and frame stay unchanged, so its full action set remains available. Both the local actor and each remote Ebisumaru slot get a private action slice; shared action data is not modified.

On the display object, only the action base `+0x38` and broad base `+0x40` change while the skin is active. The local path captures and restores those native bases when the toggle clears. The character cycler clears the toggle and restores the bases before changing the selected character. The frame update also clears the toggle if another native path switches away from Ebisumaru; it restores the old bases only while the object still carries Ebisumaru's broad-file identity. Returning to Ebisumaru requires a new L press. Native action changes may rebind the bases between frames; the hook tracks the latest native bases before applying the graft again. A model pointer that does not match the selected playable action causes a clothed fallback. Remote slots also fall back to clothed when the graft cannot be prepared.

## Controls and synchronization

The implementation reads pad 0's native pressed edge for the N64 L button (`0x0020`) from `D_8015C5C8_15D1C8 + 0x3B07C`. It accepts presses only while Ebisumaru is selected during normal gameplay with a live local player, loaded save, and no dialog, script lock, or Impact root. Selecting another character clears the toggle; entering a scripted sequence suspends the appearance while retaining it. The sender publishes appearance bit 3 only for Ebisumaru; receivers honor that bit only for Ebisumaru and rebind on either appearance edge.

## Verification

- `bash tests/run_player_skin.sh` checks toggle and restore behavior, the pure mesh rebase and action-tree patch, remote appearance transport, and render-scratch preflight.
- `python3 tools/verify_alternative_skin_assets.py /path/to/mnsg.us.decompressed.z64` checks the US ROM's broad, action, and opening file sizes; all 72 opening display references; the 14 mesh pairs; and all 232 playable Ebisumaru action trees.
- `make` compiles and links the MIPS mod. `build_mod.sh` also packages release and debug `.nrm` files.

These checks establish the code and US-ROM asset contract. A fresh in-game check is still needed for the final look, clipping around the one unmatched opening node, local action transitions, and a two-client appearance edge. A build or host test cannot prove RT64's final image.
