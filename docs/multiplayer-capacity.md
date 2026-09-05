# Multiplayer room capacity

Remote rosters, smoothing state, model slots, hit targets, and attack hit histories grow with the connected players instead of using a fixed 25-player array. Collision sweeps process the complete peer collection, including coincident spawns, without a fixed temporary array on the game thread's stack.

## September 5 rendering crash

The supplied crash report identifies the GAME thread in `func_8001904C_19C4C`, reached through the native model-tree renderer. It attempts a write at the translated address for a null record plus `0x94`.

A character's visible root is only one kind-2 record. Drawing its animated model allocates additional cached child and sibling records. The previous pool patch kept the stock 192-record backing pool and assumed one record per remote player. The native child, sibling, and root-cache constructors dereference failed allocations before their callers can handle them.

`src/global_patches/anchor_remote_model_pool.c` now adds initialized 0x98-byte records to the actual native free list as needed. The native task free list similarly gains initialized 0xf0-byte task records. Chunks are retained and reattached after the respective engine pool resets; ordinary native deletion returns individual records for reuse. Successful allocation preserves the engine's counters and list ownership. Cache construction also checks allocation failure before dereferencing a new record.

CPU records in these chunks are accepted by multiplayer pointer validation only when they belong to registered pool storage. The change does not treat arbitrary extended addresses as valid game objects.

## Shared expression textures

Face and part textures must remain in original RDRAM because native graphics commands encode their addresses. The existing 400 KiB face arena now caches immutable resource pages shared by every player using the same resource, instead of reserving four 4 KiB buffers per player. A page cannot be evicted while referenced by any model; after the final reference is released, it also survives the native double-buffered display lists.

Players retain separate animation cursors, collision state, recovery flicker, and mutable Sudden Impact action data. Sharing pixels does not merge those states.

## Drawing memory

The stock graphics bank has 540 matrices and 10,640 display-list commands. Expanding CPU records alone would eventually overflow those unchecked writers. Remote player drawing now uses separate scene scratch, split by the engine's two graphics banks. It reserves half of the remaining original-RDRAM space after character and projectile resources have loaded, leaving the other half for subsequent native allocations.

Before drawing, the mod walks the bound character's skeleton to budget commands, matrices, and the native matrix stack. Reads stay within that player's loaded model assets. Each remote receives disjoint space for its full native draw. The resulting display list is called from the stock stream; consecutive remote lists are chained to reduce stock command use. Scratch resets only when the native graphics bank is reused.

When a model cannot fit safely, that draw is skipped and a once-per-scene `[remote_render]` diagnostic records the reason. The player remains connected and active. This protects memory; it does not guarantee that arbitrarily many characters can all be visible at once. Draw capacity depends on the scene's remaining bytes and each character's model, not a fixed player count.

## Practical limits and validation

There is no player-count admission constant in these systems. Memory, drawing capacity, network throughput, and frame time remain finite. Native record counters retain their actual ABI widths; allocation failure must remain safe rather than wrap a counter or corrupt a list.

Host regressions exercise rosters and interactions beyond the former caps, shared texture lifetime, allocation failure, and pool reset/reuse. MIPS compilation and release/debug package validation check integration. Gameplay validation is intentionally left to the user; host tests and packaging are not evidence of a certified in-game maximum.

The actual C drawing-budget parser was also checked offline against all 232 USA-ROM action entries for each of the four characters and all 232 Goemon private action slices (1,160 cases). All passed the model-asset bounds checks. The largest conservative per-action budgets were Goemon 736 commands/27 matrices, Ebisumaru 672/23, Sasuke 896/34, and Yae 864/29. The native Sudden Impact substitution changes a geometry reference without changing the tree or budget; Mini changes only scale. These checks establish parser acceptance of real assets; they do not execute native drawing or certify runtime frame rate.
