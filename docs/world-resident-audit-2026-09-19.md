# Resident actor-list coverage

The room roster now includes the native resident source list at metadata+0.
Previously it enumerated only the normal and partition lists. This adds two
`0x1FC` moving platforms in room `0x30` and five `0xFC`/`0xFE` enemies in room
`0x131` to their existing world/enemy adapters. No new behavior recipe or wire
field is needed for these seven actors.

## Native evidence

US decompressed ROM SHA-256:
`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
Ghidra program: `mnsg.us.0.z64`, with symbol-table identity
`func_8020D848_5C8D18` for the actor-manager spawn routine.

- Metadata+0 supplies an already-resident instance list. The native loop uses
  an unsigned-byte index and 20-byte source stride. At `8020DA60` it stores the
  current descriptor to `D_8015CDDC`; at `8020DA68` it stores that descriptor's
  raw definition pointer to `D_8015CDE0`. `8020DA78` reads the entity directly,
  without segmented pointer resolution.
- `8020DAFC` calls `func_80218A54_5D3F24`; delay slot `8020DB00` loads the
  descriptor argument from `D_8015CDDC`. Thus the existing registration hook
  sees the same descriptor that the roster enumerated. The common initializer
  clears task+0x70; resident identity must not depend on that field.
- The entry table references at `8020DAB0` and `8020DBCC` belong to resident
  and normal spawning. `8020D560` belongs to the separate partition spawner
  `func_8020D36C_5C883C`. Resident pointers are not a second segmented alias.
- On the ROM inventory, room `0x30` appends resident slots 26–27, both entity
  `0x1FC`. Room `0x131` appends slots 7–11 with entities
  `0xFE,0xFC,0xFE,0xFC,0xFE`. These slot numbers are the mod's stable source
  order, not the native resident-first execution order.

`tools/inspect_world_roster.py` reproduces all three lists from the verified ROM:
800 metadata slots, 374 records, 333 actor-data waves, 292 nonempty rosters,
3,888 unique sources, 255 entity types, maximum 57 sources in one room.
The previous count of 333 described records with an actor-data file, not the
number of nonempty actor rosters.

## Integration and verification

Resident entries append after the existing normal and sorted partition entries.
Pointer-identity deduplication avoids double registration when lists overlap;
combined capacity is checked before recording a new slot. Changed complete
roster signatures keep clients with the old omitted list out of the affected
room's shared state. Existing packet routes, cadence, payload limits and fan-out
are unchanged; this remains world protocol 10.

The production-extracted C roster harness runs under UBSan. It checks combined
ordering, deduplication, exact 256-source combined capacity, overflow, missing
terminators, same-room death retention, missing-wave retry, resident-only
metadata, source registration, dead-spawn suppression, pool reuse and empty
script-room scope. Removing the resident traversal makes its first runtime
assertion fail by omitting the two platforms. The single-list scan limit stays
at 256 entries including the terminator, matching the existing native-bound
policy; a single list with 256 non-null entries is rejected.

Review questioned the raw-pointer and source-scratch preconditions. The native
instructions above and all seven shipped definitions confirm both. A segmented
pointer placed in metadata+0 would also violate the unmodified manager's direct
dereference contract; no support for such a modified native table is claimed.

Fresh two-client gameplay remains outstanding. This closes a source-enumeration
gap, not the remaining controller families in the main coverage worklist.

## Separate File_44 gated-branch investigation

All 33 placed entity `0x1F4` definitions still have their gate disabled; no
resident-list placement adds that entity. The native gated initializer
`func_080001B8_6FB3B8` reads byte+D4 and temporary flag index byte+D5, then
optionally emits the common visual effect and binds static geometry selected
by the same +D5 byte. It has no callers within File_44 besides its entity
entrypoint. A conservative full-symbol instruction scan found 13 calls to the
entity-child allocator `func_80217360_5D2830`, each with a constant entity ID;
none creates `0x1F4`. This narrows the remaining question but does not prove
every indirect/scripted callback creator absent. The separate dynamic gated
branch stays unclassified pending direct invocation evidence.
