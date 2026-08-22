# Anchor Multiplayer Client for Mystical Ninja Starring Goemon: Recompiled

A multiplayer mod for [Mystical Ninja Starring Goemon: Recompiled](https://github.com/N64Recomp/N64Recomp) that connects to an [Anchor](https://github.com/garrettjoecox/anchor) server to synchronise items and flags between players.

### Features

* **Item / flag sync** — every tracked item or flag gained by a player is broadcast to the team in real time.  Players who join late automatically receive the team's accumulated progress.
* **Race challenges** — race hosts can optionally sync team damage and healing, or sync both ryo earnings and spending, alongside the No Hit, 1 Life, and enemy-multiplier challenges. These rules do not run in standard multiplayer.
* **In-game HUD** — a notification banner confirms connection success or failure; a persistent player-list panel (top-left) shows every player currently in the room, their character, and their position.
* **Teams** — players can be grouped by Team Name within a room so that flag queues and save-state syncs are scoped to that team.
* **Reconnect support** — client IDs are preserved across sessions so the server can deliver queued packets on reconnect.

### What is synced

#### Items & equipment
| Category          | Items                                                                                                 |
| ----------------- | ----------------------------------------------------------------------------------------------------- |
| Characters        | Goemon, Ebisumaru, Sasuke, Yae                                                                        |
| Weapons           | Chain Pipe, Meat Hammer, Firecracker Bomb, Flute, Wind-up Camera, Ice Kunai, Bazooka, Medal of Flames |
| Weapon tiers      | Iron → Silver → Gold upgrade level for each character                                                 |
| Abilities / magic | Sudden Impact, Mini Ebisumaru, Jetpack (Super Jump), Mermaid Magic                                    |
| Quest items       | Triton Shell, Super Pass, Achilles' Heel, Quality Cucumber, Key to Training, Map of Japan             |
| Miracle items     | Miracle Star, Miracle Moon, Miracle Flower, Miracle Snow                                              |
| Warp points       | All 13 inn / tea-house / fast-travel gates                                                            |

#### Stats & collectibles
| Stat                                    | Behaviour                                                                                   |
| --------------------------------------- | ------------------------------------------------------------------------------------------- |
| HP Max                                  | Highest value wins; receiving an upgrade also refills current HP to the new maximum         |
| Current HP                              | Standard multiplayer does not sync it; the Team Damage Sync race challenge shares damage and healing deltas |
| Fortune Dolls (total & traded)          | Highest value wins                                                                          |
| Ryo (money)                             | Standard multiplayer does not sync it; the Ryo Sync race challenge shares both earnings and spending deltas |
| Fish counts (red / yellow / blue)       | Highest value wins                                                                          |
| Mr. Elly Fant & Mr. Arrow (per dungeon) | Highest value wins                                                                          |

#### Dungeon keys
All individual silver, gold, and diamond key room pickup flags are synced so that a player cannot collect a key from a room their teammate already cleared.  Lock consumption flags are **not** synced, keeping each player's key expenditure independent.

#### Story flags & progression
Boss defeats, character/ability acquisition flags, quest-chain milestones, NPC-unlock flags, post-boss cutscene flags, and Gorgeous Stage / witch cutscene flags are all synced so teammates never get locked behind progression gates their partner has already passed.

#### Fortune doll room pickups
Every individual silver doll (40 rooms) and gold doll (5 rooms) room pickup flag is synced, preventing any player from re-collecting a doll from a room a teammate already cleared — which would otherwise inflate the doll count and give unbalanced HP upgrades.

### Networking

Networking is handled by `py/anchor_mnsg.py`, a Python module that runs inside the [REPY (RecompExternalPython)](https://github.com/LT-Schmiddy/zelda64recomp-python-extlibs-mod) extlib embedded in the `.nrm`. REPY must be installed as a dependency mod.

Player movement is coalesced to a steady 5 Hz update rate, with action/animation edges and sparse motion starts, stops, and reversals sent immediately. Receivers keep only the newest movement state per player instead of adding movement packets to the gameplay-event queue. A native-order client predictor [reconstructs the 30 Hz game ticks between those snapshots](docs/remote-player-movement-interpolation.md) from final linear/angular displacement and the source animation's actual per-tick rate, with signed arrival phase, bounded constant residual correction, conservative shortest-path rotation, and bounded packet-underrun prediction.

Join-time item synchronization uses one compact team-state snapshot followed by durable incremental flag packets. This avoids the previous burst of hundreds of individual packets when a save loads or a teammate joins.

The public default server is **anchor.hm64.org:43383**.

Room IDs are required and have no shared default. Choose a unique private value and share it only with the people you want in the session. The client adds the hidden `mnsg-` game namespace when connecting. Players must use both the same Room ID and the same Team Name to share and sync game progression.

### Configuration

All options are available in the mod settings menu inside the game:

| Option                  | Default           | Description                                                                    |
| ----------------------- | ----------------- | ------------------------------------------------------------------------------ |
| Server Address          | `anchor.hm64.org` | Hostname or IP of the Anchor server                                            |
| Server Port             | `43383`           | TCP port of the Anchor server                                                  |
| Room ID                 | *(blank)*         | Required private code that keeps multiplayer sessions separate                 |
| Player Name             | `Player`          | Display name shown to other players                                            |
| Team Name               | `default`         | Players with the same Team Name share and sync game progression                |
| Show NET Button         | Enabled           | Show the NET button in the bottom-left corner                                  |
| Show Item Notifications | Enabled           | Show a toast when items or flags are received or found                         |
| Show Room ID (Hex)      | Disabled          | Display the raw hexadecimal room ID next to the area name in the player list   |
| Show Player Positions   | Disabled          | Display each player's X, Y, Z world position next to their name                |

### Dependencies

* [RecompExternalPython (REPY) v2.0.0](https://github.com/LT-Schmiddy/zelda64recomp-python-extlibs-mod) — provides the embedded Python 3 interpreter used for TCP networking.

### Tools

You'll need to install `clang` and `make` to build this mod.
* On Windows, using [chocolatey](https://chocolatey.org/) to install both is recommended. The packages are `llvm` and `make` respectively.
  * The LLVM 19.1.0 [llvm-project](https://github.com/llvm/llvm-project) release binary does not support MIPS correctly. Install **18.1.8** instead — specify `--version 18.1.8` with chocolatey, or download the 18.1.8 release directly.
* On Linux, these can both be installed using your distro's package manager. You may also need to install your distro's package for the `lld` linker. On Debian/Ubuntu based distros this will be the `lld` package.
* On MacOS, these can both be installed using Homebrew. Apple clang won't work, as you need a MIPS target for building the mod code.

On Linux and MacOS, you'll also need the `zip` utility.

You'll also need a build of the `RecompModTool` utility from the releases of [N64Recomp](https://github.com/N64Recomp/N64Recomp). You can also build it yourself from that repo if desired.

### Building

* Run `make` (with an optional job count) to compile the mod code.
* Run the `RecompModTool` utility with `mod.toml` as the first argument and the build directory (`build`) as the second argument.
  * This produces the `mnsg_multiplayer_anchor.nrm` file in the build folder.
  * On MacOS, you may need to specify the paths to the `clang` and `ld.lld` binaries using the `CC` and `LD` environment variables.
* Alternatively, run `./build_mod.sh` to produce both package variants in one command:
  * `build/mnsg_multiplayer_anchor.nrm` is the release build with the debug button disabled.
  * `build/debug_mnsg_multiplayer_anchor.nrm` is the same mod with the debug button enabled.

### Updating the Mystical Ninja Starring Goemon Decompilation Submodule

This mod can be rebuilt against newer versions of the MNSG decompilation. To update the targeted commit:

* Build the [N64Recomp](https://github.com/N64Recomp/N64Recomp) repo and copy the `N64Recomp` executable to the root of this repository.
* Build the version of the MNSG decompilation you want to target and copy the resulting `.elf` file here.
* Update the `mnsg` submodule to point to that commit.
* Run `N64Recomp generate_symbols.toml --dump-context`
* Rename `dump.toml` → `mnsg.us.syms.toml` and `data_dump.toml` → `mnsg.us.datasyms.toml`, placing both in the `Goemon64RecompSyms` folder.
* Try building.
  * If it succeeds, you're done.
  * If it fails due to a missing header, create an empty file at `include/dummy_headers/<path>`.
  * If `RecompModTool` complains that a function "is marked as a patch but not existing in the original ROM", find the function in the old decomp's map file, locate it by address in the new map file, and update the name in your source.
