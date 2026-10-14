# M35: original Leon action corrections

Reference: original source frozen at `9b76ff003081d6ece8b838453fb8310743a972c7`. The complete reviewed definition/dispatch inventory is in `PLAYER-ACTION-COVERAGE-M35.json`; source SHA256 values pin all 91 enumerated files. Its 1,820 definitions and 54 dispatch tables are an inventory, not a completion percentage.

## Initial firearm and knife pitch

`CameraControl::getCameraPitch()` in `game/cam_ctrl.cpp:1279` returns zero. `PlSetLockPitch` uses that getter during ordinary firearm draw, and the knife draw uses the same original pitch basis. The native adapter previously seeded its blend rate with exploration-camera pitch. Looking at the floor before raising a weapon therefore made the weapon point at the floor.

New WEP01, WEP02, WEP07 and knife draws now reset the initial aim rate to zero. The original horizontal camera-direction fold during draw is retained. Independent right-stick aiming remains active after draw, so the player can deliberately aim down or up while holding a weapon.

## Long-gap jumps

This is the distinct original `jumpCheck` / `jumpFallOn` / `pl_R1_JumpFall` route, not an AEV ladder, an ordinary ledge drop, or a broken-window vault. `r10c` supplies authored SAT bit19 walls in SAT0; additional dynamic crate subslots are not assumed to exist in a fresh room.

The offer uses both original shoulder rays at local X±300, Y400, length600, intersects their attributes, and requires bit0x80000. The second hit supplies the original direction. Its floor probe is 3800 beyond the hit and 1500 up, with the source 600-above/100000-below interval and ±1500 height limit. The native offer also checks the authored landing against real scenery to avoid restoring collision inside a wall.

The action uses actual PL archive entries90/92 (`PL_ARC5e/60`) and91/93 (`PL_ARC5f/61`). It keeps source height adjustment on work ticks5..14, checks the previous tick's part4 world Y before motion movement from sequence frame7, advances the newly installed landing in the contact tick, and releases control at landing sequence frame20. It adds no alignment teleport or synthetic ground.

Eight actual `r10c` offers complete in both directions, including the four opposite river approaches near X49000 and four elevated diagonal approaches near X24000. Every native pose and root step was compared to an independent replay of the original FCV/SEQ, including contact order, sound events and final landing. All eight complete without rollback.

## Original motion sound events

`AnimationRig::consumeSequenceSound` exposes and clears only original `Motion.Seq_old.Se`. `LeonPreview` consumes it after each native simulation tick, before the bridge's world-tick callback. It retains Se−1, Free&7, exact source part21/25/root selection, solved world position and root position for companion clothing sounds. Zero-time updates, display-only half ticks and paused item examination emit no new motion event.

The bridge maps these through `SourceFootSoundCue` and `FootstepSounds`. The environment owner resolves the source FSE block5 dry/wet and material variants. Original sand-power effects and the global foot gameplay flag remain outside this audio adapter.

## First r100 axe damage

The selected enemy attack supplies original power380 and its original front/back/height result0..5 to `receiveGanadoAxeHit`. Normal Leon uses the unarmoured `LifeDownSet2` rules and RNG consumption. The configured original rank scales damage by `float(rank)*0.1f+0.5f` with original truncation; Easy ranks≤2 automatically retain one HP above300, while still consuming the original random survival roll. `PlSetDamage`'s extra zero-damage RNG call is retained where the original calls `setDamage`, and is skipped for a fatal middle/low hit that directly enters Die0. The same source scaling applies to existing grabs and blast damage, and rank survives room reconstruction.

The following original clips are used: front PL48, back PL49, middle/low front PL4a+64, middle/low back PL4b+65, fatal PL4c+4d. Motion starts in the next player routine tick, with the source five-frame blend and flags1. The protected damage timer, work30/35 input cancel and five-tick recovery are retained. Damage keeps direction123; it does not invent a turn toward the attacker.

Campaign health is mortal. The optional unlimited-health test setting keeps damage reactions and RNG while preventing HP loss. All six living hurt variants and four fatal upper/middle/lower cases were checked against independent original motion replay. Selected ranks2/5/10 were checked for HP, RNG and reconstruction inheritance. Armour and the full adaptive `GameAddPoint` rank manager remain outside this selected damage helper.

## Validation and remaining scope

`m35_player_actions_check` passes 4,727 checks with supplied original PL00/WEP01/WEP02/WEP07/EM12 and real `r101`/`r10c` resources. Existing player motion tests, the M34 shotgun flow check (1,010 checks), and the exhaustive grab-damage oracle (4,587,520 cases) also pass. `python3 tools/audit_player_actions.py --check` verifies the updated inventory reports without changing the historical M34 reports.

These changes do not establish complete original gameplay parity. Unsupported or partial paths remain explicit in the M35 inventory: ledge climb/descent, contextual crouching, boat control, whistle/partner commands, automatic general falls, injured gaits, cloth/neck/floor IK, all other weapons and tuning, aim sway/recoil RNG, shell ejection, full scenery/water hit dispatch, all enemy grabs/QTEs, all damage types, original game-over presentation and the complete scenario/event scheduler. JumpFall's campaign disable flag and r226 height exception are not installed. The native collision/landing and camera owners remain adapters rather than a transfer of the whole original cAtari/CameraControl runtime.
