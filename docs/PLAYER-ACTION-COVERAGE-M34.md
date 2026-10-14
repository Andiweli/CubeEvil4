# M34 – Leon/interaction source coverage

**Not all original actions are identical or implemented.** This report makes the missing work explicit; opening a room or having an animation asset is not evidence of full gameplay parity.

Frozen source reference: `9b76ff003081d6ece8b838453fb8310743a972c7` (`docs/PORTING.md`). Every enumerated source file is also pinned by SHA256 in the JSON report.

The machine inventory covers 91 source files, 1820 callable definitions and 54 original dispatch tables. Definition counts are an inventory, **not a completion percentage**.

## All top-level movement slots

| Slot | Original function | Current native coverage |
| --- | --- | --- |
| 0 | `pl_R1_Footwork` | Original normal idle FCV; neck/injury/whole state machine incomplete. |
| 1 | `pl_R1_Walk` | Original normal walk/root/turn; collision/IK adapter. |
| 2 | `pl_R1_Back` | Original normal back-step/root/turn; collision/IK adapter. |
| 3 | `pl_R1_Run` | Original normal run/root/turn; neck/injury/breathing incomplete. |
| 4 | `pl_R1_Turn` | Original normal in-place turn; character/injury variants incomplete. |
| 5 | `pl_R1_Turn180` | Original normal quick-turn with frame19 cancel; full camera chain partial. |
| 6 | `pl_R1_Weapon` | Three playable gun profiles; all other weapon classes and tuning absent. |
| 7 | `pl_R1_LevelUp` | Absent: original ledge-climb dispatcher/motions. |
| 8 | `pl_R1_LevelDown` | Absent: original ledge descent/1m dispatcher. |
| 9 | `pl_R1_ObjPush` | Tested room racks and original FCVs; general manager incomplete. |
| 10 | `pl_R1_Aux` | Specific ladder/EM12 callbacks; arbitrary original callbacks absent. |
| 11 | `PlKnifeMove` | Original knife FCVs/sweeps for tested targets; lock turns/others partial. |
| 12 | `pl_R1_Fance` | Original wide/narrow fence and broken-window branch; bounded landing adapter. |
| 13 | `0` | Original unused/null slot. |
| 14 | `pl_R1_Fall` | Original tested ledge fall/land; automatic falls/water branch incomplete. |
| 15 | `pl_R1_Boat` | Absent: boat control and weapon dispatcher. |
| 16 | `pl_R1_Ladder` | Original fixed tower profile; all arbitrary lengths/callers incomplete. |
| 17 | `pl_R1_Crouch` | Absent: contextual crouching and crouch weapon flows. |
| 18 | `0` | Original unused/null slot. |
| 19 | `pl_R1_JumpFall` | Absent: separate source long-jump/fall action. |
| 20 | `pl_R1_Whistle` | Absent: partner-command whistle flow. |

## Other required original gameplay work

- Original generic scenario/event scheduler and camera-cut control; context-specific action cameras do not install it.
- Directional normal damage, complete enemy grabs/QTEs and fatal state handling; unlimited HP remains the requested test override.
- Source weapon sway/recoil RNG, all weapon classes/tuning, shell ejection and complete water/scenery/material impact dispatch.
- Original cloth/neck/floor IK; current motion IK floor callback does not expose the world SAT floor.
- Shoot-down, event/enemy-gated items, keys used by scenario, yellow-herb max-HP upgrades, partner use and attachment state.
- Treasure chests, rolling barrels and complete room-specific destruction/event callbacks.
- Native subscreen presentation currently uses original disc assets through a custom controller UI; full original menu animation/transition, save/load and campaign state are separate work.

## Source-backed corrections in this patch

- Gun recoil enters the original set40 tail using source sequence frames. Releasing aim goes directly to weapon-down without installing another aim or draw motion.
- Shotgun aim uses source weapon7 pitch/body speed0.8 and six-degree waist limit instead of the handgun profile.
- Case Combine supports the original selected-ammo-stack reload in both selection orders, preserving equipped weapon and packing.
- Ammo labels follow the actual weapon-to-ammo table: 0x07 belongs to the WEP09 rifle; 0x00 belongs to the magnum profiles.
- Broken-window directions0/1 use original narrow fence FCVs and r3=5 (no fanceAdjust), with original frame30 movement cancel and source footstep cues. Glass is not re-created or re-broken.

Detailed per-function limitations and every extracted dispatch entry are in `PLAYER-ACTION-COVERAGE-M34.json`. Re-run `python3 tools/audit_player_actions.py` to regenerate both files.
