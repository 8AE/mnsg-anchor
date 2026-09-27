# Remote player visual effects

Remote player effects use the sender's current room, interaction session, player
epoch, action and appearance snapshot. The existing `MNSG_PLAYER_POS` path sends
action and appearance changes immediately; receivers do not create gameplay
players or invoke attack, damage, collision or item award callbacks.

| Effect | Native source | Remote rendering |
| --- | --- | --- |
| Character switch smoke | file_11 `func_801EA860_5A6770`, selector 3, six `0x490024F0` models | Visual-only six-part replica. Each puff carries the native camera-facing `0x8000` rotation markers. The native child callbacks use the local target global, so they are not replayed. A received character change also starts smoke if action `0xBA` was missed. Late snapshots can start it, and resource or bank contention is retried briefly. |
| Mini Ebisumaru shrink/grow and Yae Mermaid enter/exit purple effect | file_11 `func_801F11F0_5AD100`, selector `0x22`, four children | Original visual constructor runs under an inert native manager owned by the remote render task. Actions `0x8E`, `0x8F`, `0xAA`, `0xAC` trigger one burst even when first received after animation frame 3. A blocked burst is retried while its action remains active. |
| Fire Ryo/camera/ice kunai charge | file_11 `func_801DC27C_59818C`, selector `0x11`, models `0x1900012C`, `0x1900025C`, `0x1900037C` | Appearance bits 6 and 7 carry active/full charge in the existing position snapshot for Goemon, Ebisumaru and Sasuke. A visual-only renderer follows the owner's pose and writes the native material wrapper. It does not run the native callback that queues charge sounds. |
| Sudden Impact aura | file_11 `func_801F15E0_5AD4F0`, selector `0x23`, main `0x190006B8` and five `0x1900074C` children | The original visual constructor runs after Goemon action `0x82` reaches frame 4. A receiver that first sees action `0x83` with the gold appearance bit can still start the aura once. Its owner is the inert remote render task with synthetic action/character bytes. All six child display records are rebound to the staged remote Goemon broad resource (file `0x120`). |
| Sasuke jetpack smoke | file_11 `func_801EDC08_5A9B18`, selector `0x16`, `0x490026B0` puffs | Visual-only twin emitter with bounded puffs. Each puff carries the native camera-facing `0x8000` rotation markers. The position snapshot carries native task `+0xA4` vertical speed in hundredths during Sasuke action `0x9B`. Active smoke uses the native drift formula; departing smoke uses the native fade drift. The native callback writes player work and queues a sound stop, so it is not replayed. |
| Ryo pickup burst | file_12 `func_8021804C_5D351C`, slot-8 child effect | An accepted fresh room coin tombstone starts this visual constructor on the still-live mapped coin actor before removal. It never invokes the pickup callback that awards currency. |
| Thrown Fire Ryo trail | file_11 `func_801ED1F0_5A9100` and `func_801ED26C_5A917C`, model `0x4900B878` | Native kind `0x12` coin is captured after its first flight tick and sent through the existing transient room projectile route. The receiver simulates the visual coin, impact and up to four trail particles. Its trail binds the staged remote Goemon file `0x120` and resident texture file `0x152`; it never runs the coin's native collision callback. |

`MNSG_PLAYER_POS` is latest-state room traffic. Charge start, full, and release
edges bypass the normal movement cadence without per-frame particle packets.
The optional `jetVelocity100` scalar uses the same position route and cadence.
Missing or invalid values clear the prior sample; older clients retain the
previous jet smoke fallback. New clients use the transmitted native speed.
`MNSG_PROJECTILE_SPAWN` remains a quiet transient room broadcast; Fire Ryo uses
the existing bounded payload, TTL, peer session/epoch validation and event ID
deduplication. Its visual trail adds no network messages. The receiver can
render at most four Fire Ryo shots with four trail particles each.

The shared kind-2 pool starts with 192 records and grows in 64-record chunks.
The new player effects cap smoke at two
banks of 20 records, purple managers at four bursts of four, charge at two
three-object groups, and aura at two six-object groups. Smoke tasks release
their bank at frame end after their last puff retires. Native constructors and
visual object allocation can decline when the game's pool has no free record.

Ryo pickup particles run under the independent native category-8 task list.
The source coin supplies pose and metadata, so removing that coin does not
delete the visual task. The coin's native pickup callback
`func_802145F0_5CFAC0` awards currency; remote replay calls only its visual
constructor `func_8021804C_5D351C(actor, 0)`.
Up to eight remote pickup tasks have their 24-byte `object+0x80` materials
mirrored into low RDRAM immediately before drawing. Their native cleanup
callbacks remain intact. The native task-reset hook clears tracked identities
before a task address can be reused for a local effect.

## Native lifecycle and graphics memory

Character switching sets player work `+0x69` and later clears it while action
`0xBA` remains active. Both changes advance the combat player epoch. Smoke
preserves one switch burst across these verified boundaries within the same
room/session; joins, reconnects and room changes establish a fresh baseline.

The scheduler uses one flat depth-first task list. Aura traversal stops when
the depth returns to its manager's depth, leaving following unrelated tasks
alone. CPU task/model records can live in the expanded pool, but GPU display
lists must remain in the original 8 MiB of RDRAM. Purple and aura children
therefore mirror their native `task+0xA4` materials into reserved RDRAM after
each native visual update. Graphics buffers follow the native graphics bank,
not an independently toggled update counter. Stage reload retires owned
children before replacing their material arenas.

The switch and jet smoke renderers preserve `0x8000` in object rotation fields
`+0x14`, `+0x16`, and `+0x18`. The native smoke model uses those markers to
align each puff to the local render camera. The remote replicas restore the
markers on every active draw; owner yaw still controls particle placement.

These contracts were checked against native disassembly and host fixtures.
Build and package checks do not establish live two-client visual correctness;
the user performs the in-game testing.
