# File67 local travel choices and shared progression

File67 entity0x335 in rooms0x14C/0x158 owns a local camera/player sequence
ending in a warp to stage0xA8. Its four Miracle inventory prerequisites are
shared progression. The departure decision and a consumed dialogue cue have
different lifetimes and must stay local.

## Native evidence

US ROM SHA-256:
`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
Programs `/codex-temp/world_file_67.elf` and `/mnsg.us.0.z64`.
Raw audit: `/tmp/mnsg-world-sync/file67-70-native-audit-2026-09-19.txt`;
scenario follow-up: `file67-scenario-followup-2026-09-19.txt`.

Dialogue0x1F9 is in File102. Its affirmative choice reaches opcode0x8020,
argument0x6B at ROM0x77E140. The negative branch does not set or clear that
bit. `func_08000090_729630` phase5 waits for dialogue completion and reads
save0x6B: clear runs the local helper/camera cleanup, set starts departure.
Thus a remotely inserted6B can override the player's negative answer.

File102 also sets0x6C in four dialogue branches (ROM0x77DDB0,0x77DE1C,
0x77DE80,0x77DEEC). File67 phase3 reads and clears6C after dialogue1F6.
The following camera motion executes regardless of that read;6C is consumed
local scene state, not a monotone completion record.

The full direct native helper scan found no other constant callers for6B/6C;
the aligned scenario-opcode scan found these writers. These scans do not prove
that indirect accesses are absent. The concrete local choice and clear paths
already rule out treating these bits as unconditional shared acquisitions.

## Replication change

Remove `fl_outerspace` and `fl_to_space` from `s_flag_bits` in `item_sync.c`
and from the debug sync catalog. This single production allowlist governs:

- Incremental gain detection and queued `SET_FLAG` publication.
- Full outgoing `MNSG_TEAM_STATE` and `UPDATE_TEAM_STATE` snapshots.
- Incoming direct/queued `SET_FLAG` name lookup.
- Incoming live/stored snapshot key enumeration.

Old keys are ignored even if a server has already queued them. No server queue
deletion, new packet, protocol word, polling rate or fan-out is introduced.
Other item packets retain their existing team route and durability.

Native save writes, clears, dialogue, camera and warp callbacks are unchanged.
The four Miracle words at save offsets0x250/0x254/0x258/0x25C remain shared.
File70 entrance completions0xC3/0xC4 and Gateway progression0x17 remain
durable shared flags. Ordinary healing and ryo retain their existing policy.

## Validation boundary

The focused regression compiles the whole production `item_sync.c` with host
transport/UI/native stubs and links the real JSON/string/reconciliation helpers.
It calls the production apply, monitor and snapshot functions directly. It exercises local-bit
preservation, legacy-input rejection, outgoing exclusion and retained shared
progression. Its pre-change table must reproduce the remote-choice defect.
The socket packet drain and per-frame hook are not executed. This validates the
C replication boundary; it does not execute the actual File67 camera/player
callbacks or prove a fresh two-client scene run.

Existing saves are not rewritten. An already-set6B may represent a legitimate
local choice or an old remote packet; its origin cannot be reconstructed from
the bit. A local negative choice also does not natively clear existing6B.
This change prevents future remote contamination and preserves native save
semantics. Both clients must use the updated build to enforce that boundary.
