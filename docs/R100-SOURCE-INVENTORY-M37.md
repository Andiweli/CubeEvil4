# M37: complete r100 source inventory

This is a coverage inventory, not a claim of complete engine parity. The reference is the frozen `upstream/re4` commit `9b76ff003081d6ece8b838453fb8310743a972c7`, especially `src/st1/r100.cpp`. The actual Disc1 assets contain 40 AEV records, 24 ITA records, 25 ESL records assigned to room0x100, 17 SST banks containing236 records, and14 EAR volumes. Counts include inactive source records; they do not authorize spawning them early.

The module defines exactly31 functions. Every definition is listed below. Source comments sometimes use an outdated event name: the executable `r100_evtName` table is authoritative. “Implemented” here means the stated native adapter exists and has its listed bounded checks; it does not mean all dependencies of the original global engine are ported.

## All module routines

| Source function, source line | Native owner / behavior | Coverage and limits |
|---|---|---|
| `R100Init`,114 | `R100Scenario`, `R100Secondary`, bridge preparation, RoomAreaEffects, EnemyGroup, DogTraps | Actual models, source initial flags, manual first Ganado, callbacks, scripted AEV and SST17/18; full original allocation/module/task managers replaced by native owners |
| `R100Main`,359 | Secondary per30Hz tick | Area6 event prefetch/RSF0, areas7/8 RSF1, RSF12 battle-stream fade, officer talk/idle/countdown; source stream manager tested separately |
| `readEvent`,417 | `OriginalEvent` preparation / asynchronous room loader | Actual EVD/BIN/TPL/FCV/SEQ/EFF assets; original module memory swapping becomes immutable ownership, not console address relocation |
| `freeEvent`,481 | Event/room owner lifetime | Cut/event effects and actor GPU owners removed; console DC swap/free mechanics are not reproduced |
| `r100_em_set`,496 | EnemyGroup | Exact seven ESL indices3/4/5/12/13/26/27 and four EM12 body families; complete EM10 AI remains incomplete |
| `r100_GakeEvent`,508 | Secondary phase9 / camera9 repeat | First source event index4 is **r100s30**; repeated inspection uses CAM9/MDT28 and preserves Leon position |
| `r100_StartEvent`,550 | Opening chain / radio entry | r120s00→r120s01→r100s40, SCF59 cancellation, original gate placement and term invocation0; radio integration has separate verification |
| `r100_DoorCk`,606 | RoomInteractions / original door fiber | Door ETS2 locking, Ganado/flag31 closure, AEVB examine toggle and release; actual source-fiber proof32,389 checks |
| `r100_WindowBreakCk`,662 | RoomInteractions + scenario release | ETS0/1 source fence state, RSF10, house FLAG64/RSF4 and first-three Ganado flag80; source door/window proof covers glass and jump state |
| `r100_HouseEvent_exit`,699 | Secondary house camera exit | Destroys transient ESL25, restores camera and NoSuspend |
| `r100_HouseEvent`,709 | Secondary AEV15/CAM10 + source transient motion adapter | RSF15 once, Leon ROOMARC34, source event spawn/set1B and CAM10 end151;310 ticks/127,720 exact pose floats,104 CPU checks |
| `r100_StreanChk`,757 | Secondary audio state | Source village and chase gating/cooldown150; global RNG ordering with every original manager remains incomplete |
| `r100_Sce_look`,842 | Scenario first-look + OriginalEvent s03 | Actual AEVA, source placements and21 authored cuts; source English caption suppression is intentional |
| `r100_Sce_zombi_dead`,890 | Scenario/death release + Secondary/truck + radio | Waits for original ACTIVE clear, spawns seven before s20, source4/5 release, ravine cars/traps, dynamic corpse examine and RSF10/SST17; native camera and global event managers remain adapters |
| `r100_Car_pos_move`,969 | Secondary actual SMD/parts | Exact ravine positions, part rotations/scales, hidden officers, source SMD visibility and AEV4/26/27/5/25 switches |
| `r100_trap_set`,1027 | DogTraps + cliff adapter | Eight actual dog/trap ESL owners9/A/B/C/D/E/1D/7A; game-count0 also enables cliff6/7/8; source cAtari/EAT dynamic manager is bounded |
| `r100_MesDoor`,1056 | Secondary AEVB | Source knock sound6/29, MDTB and door script enable state |
| `r100_MesTruck`,1063 | Secondary AEVC | Source examine message and posttruck disabling |
| `r100_MesGanado`,1069 | Dynamic corpse AEV | ±750 quad, height1000, priority1, press8; lookdown camera uses Leon parts32/33 mean and corpse part2, FOV45, MDT D attributes1012 |
| `r100_MesCar00`,1080 | Secondary phase6 | AEV19→r100s41, repeatable before RSF10; source car/cop suspend and return |
| `r100_MesCar01`,1109 | Secondary phase7 | AEV1A→r100s43, repeatable before RSF10; source car/cop suspend and return |
| `r100_MesBrige`,1139 | Secondary phase9 / camera8 repeat | AEV18 aftertruck uses first r100s30 path/RSF14, later CAM8/MDTE |
| `r100_EventBrige`,1158 | Secondary phase8 | AEV1B→r100s44 before RSF10, source return flags |
| `setTexRender`,1200 | Environment / EnvironmentGl | Actual EST0 64×64 target; SMDworks0/1 alpha240 shader2 refraction power30/ratio128; original pond retained independently of SST effects |
| `Evt_R100S40_Func`,1223 | OriginalEvent callback + event mirror/car | Source camera-in-room flag, cut0/frame0 EventCarInit and cancellation SCF59 |
| `Evt_R100S20_Func`,1244 | OriginalEvent callback + EnemyGroup script ticks | Truck visibility cut0, Ganados4/5 NoSuspend at cut2; five actual cuts |
| `Evt_R100S03_Func`,1271 | OriginalEvent handgun visibility + source CMF5 | Actual wep0200 FN57 handgun is visible cuts0…4/13…20, hidden5…12; all21 cuts checked. The source comment calls it a knife and reverses visibility, but executable `be_flag`/`cModel::isTrans` behavior is authoritative |
| `r100_mes_gaikotu_bgm`,1310 | Secondary/SoundMixer | Room volume1 over200, voice0/12, source wait/status/cancel and restore2D over200 |
| `r100_mes_gaikotu`,1326 | Secondary AEV16/MDT + skull voice | Source FLAG29, upcut2F/MDTB and completion handoff |
| `r100_mes_gaikotu_bgm_down`,1335 | Secondary AEV22 | Room stream volume2D over400 |
| `r100_mes_gaikotu_bgm_up`,1341 | Secondary AEV23 | Room stream volume reset over400 |

## Complete authored AEV index coverage

The IDs below are hexadecimal. The type names are from `include/sce_at.h::SCEAT_ID`; typeB is runtime scenario collision, and typeD is field/in-room information. Neither is an interaction message.

| AEV IDs | Original type / purpose | Native owner / remaining dependency |
|---|---|---|
| 00 | Door/room exit | RoomFlow: actual pressed/heldA and authored front geometry →r101; no kill gate. r102 is reached later from r101 |
| 01,02 | Execute | Secondary authored ravine/room scripting; source enable flags retained |
| 03,09,24 | Flag | Scenario door closure and house FLAG64/other FLAG65; exact authored area geometry |
| 04,05,1C,1E,25,26,27 | SCR_AT | K13 ScenarioCollision creates all seven original unparented areas, with literal atari.cpp triangles, SAT/EAT attributes and pre/post-RSF10 switches; shared walk/camera/shot queries include them. Parented areas and the complete original cAtari/global managers remain outside this adapter |
| 06,07,08,0D | Normal detection | Scenario/Secondary prefetch, departure and audio areas |
| 0A,0B,0C | Execute | First look, locked house door message, truck message |
| 0E,0F | FIELD_INFO | Authored indoor volumes are available; complete original actor State/lighting/camera propagation remains bounded |
| 10,11,12,13 | Message | Actual MDT11/12/13/14;13 has authored camera8 |
| 14 | Save | Root native typewriter/card workflow; source BE save codec separately checked; full fresh global-manager state is incomplete |
| 15 | Execute | One-time house scare / CAM10 / transient ESL25 |
| 16,17 | Message | Skull override MDT2F/cameraB; country-dependent MDT31 |
| 18,19,1A,1B | Execute | Posttruck ravine, officer events s41/s43, pretruck s44 |
| 1D,1F,20,21 | Message | MDT30/32 and authored cameras14/16/15 |
| 22,23 | Execute | Skull-area room music down/up |

The corpse examine area is created dynamically after the first Ganado dies and is additional to these40 immutable AEV records. ITA is independent: all24 actual item records are decoded and checked, including parent/dead-crow activation, original pickup bounds and original effect0 behavior.

## Every actual r100 ESL record

`etc/emleon00.esl` has255 total32-byte slots, of which exactly25 specify room0x100. Source-controlled inactive slots stay inactive until their particular routine invokes them. The first manual Ganado uses the event sentinel0xFF and is additional to the file's records.

| Actual indices | Source role | Owner / proof |
|---|---|---|
| 00 | Inactive EM12 type0/set0 | Preserved source record; R100Init instead creates its explicit event Ganado with set13/flag21000020/HP500 |
| 03,04,05,12,13,26,27 | Seven ambush EM12 | EnemyGroup; original weapons/body families, source4/5 WalkStay and later house-boundary release |
| 06,07,08 | Cliff falling EM12 | Exact source wait→fall→fade;673 ticks/277,276 pose floats,708 CPU/50 GLES checks; HP0 is source script state, not normal dead/drop |
| 09 | Rescue dog EM21 type1/set1 | DogTraps; source Leon185-frame rescue, SCF12 and jingle1/E |
| 0A,0B | EM2A wire explosives | Source HP2160/1860, exact posed wire geometry/contact, source three-tick delay/two blast centers |
| 0C,0D,0E,1D,7A | EM2A bears | 0C source dog-held set1; other four ordinary bears; exact open/closed poses, source catch/contact and persistence |
| 0F,10,11,20,21 | Five initially active EM23 crows | ForestAnimals; original FCV/SEQ/posed target shapes;10 drops ITA9 Spinel,20 drops ITA10 grenade |
| 25 | Transient house scare EM12 type0/set1B | Source Wait89→Turn31/ROOMARC20 blend10→Walk32/ROOMARC21 blend7; removed on house camera exit |

No deer record belongs to this actual room. Source dead flags and item/dead-actor associations are separate from general visibility.

## All SST/EAR and registered effect sources

| SST IDs | Source area | Records |
|---|---:|---:|
| 00,01,07,08 | Global FFFF | Screen jitter, three cloth sheets,60-particle weather burst and Filter03 controller |
| 02,11 | Area6 |20/6|
| 03,09 | Area2 |1/15|
| 04 | Area9 |28, including4 COREpath10 followers and actual Esp08 shimmer |
| 0A | Area1 |26|
| 0B | Area0 |7|
| 0C | Area5 |15|
| 0D,12 | Area4 |21/34|
| 0E | Area3 |28|
| 0F,10 | Area7 |21/7|

All236 records are retained/enabled: Esp00=113,04=1,06=4,08=8,0A=3,0B=3,15=1,46=1,48=99,4E=3. RoomAreaEffects owns all14 EAR volumes, original+100 height tests, source display flags17/18, creation/deletion and shared512 pool. It runs every generator before every sprite and preserves global room visual RNG ordering. Global warmup is200 frames. Room camera warmup uses the destination-local native follow camera; this is not a complete original camera engine.

The source `EatMgr.registEffInfo` registers types2/4/5. Effect attributes use bits800000/8000/80; ordinary gun lookup selects the **fifth pair `effGun`**, not the first pair. Type2 gun impacts therefore select ROOM20; type4 selects ROOM0E; type5 is EFF_NONE and its flag2 drives original DontFire. `R100RoomHitEffects` now implements actual ROOM20 Esp40, ROOM0E Esp05/00, CORE1F/20 and puddle11, retaining36 original records. All eight pairs are inventoried/tested; unported explosive selections are returned as explicit original requests with diagnostics. `wepSetWaterShot` cross-plane wave pushes and full pool stealing remain separate dependencies. Generic trap/blood effects do not prove surface-hit dispatch.

Other effect owners are actual event EFF, weapon EFF, EM12 blood, EM2A dry traps/FE destruction and CORE item effects. Original water-only EM2A EST6/Esp40 and Ashley-specific7/8 are outside the verified dry Leon trap route. Blood Esp07 for Leon's bear catch is implemented and checked7,628 observations. The complete global RNG chain between every independent effect/combat manager remains unported.

## Evidence and declared limits

| Actual source path | Latest bounded evidence |
|---|---|
| Opening CPU timeline |71,652,856 checks from the final frozen build; r120s00 1,970 frames/9 cuts, r120s01 2,360/15, r100s40 1,175/8 |
| Opening CPU effects |5,329,415 checks;61,900 model observations/8,280 sine/22,000 trail ghosts/675 lights/84,483 screen/37,440 bilinear |
| Room SST/EAR |7,918,830 checks;387,308 quads/73,920 path observations/138,000 weather observations;44 independent path followers over1500 frames, moving-camera weather oracle500 frames |
| Cloth |526,972 source grid/wind/normal comparisons over40 frames,184 CPU and34 GLES checks |
| Dry trap CPU effects |62,246 checks;2,340 line/32 lens/7,628 caught-blood observations and10 exact mixed FE seed/placement emissions |
| Ordinary surface impacts |74,019 checks;36 actual records/120 exact flutter observations/530 quads/7 detached water sprites;3 tables and5 frozen source bodies |
| Dog/traps |285,653 CPU and152,484 GLES checks, all8 original owners; maximum measured GPU matrix error0.000138256 source units |
| Crows |75,591 CPU and187,346 GPU checks |
| Item effects |4,508 CPU /24 GLES checks; all24 actual ITA records and15 CORE banks |
| Source gun damage |43,344 profiles/173,376 exact integers for3 weapons/7 powers/source RNG, plus mixed-actor nearest-ray bridge and finite miss segment checks |
| House/transient/cliff |Original source pose oracles above; complete initial encounter3,383 checks |
| EM12 awareness/routes/peers |18,343 CPU checks; actual66-point/160-link/4,356-next-hop RTP; Find and peer functions byte-equal to source; full dynamic route action state remains incomplete |
| EM12 dry steps/death/scenery capture |8,527 checks /111 actual SEQ sounds across8 combat profiles; source Timer49 drop, one-time Die_Cramp kill phase and captured13-ray scenery normals |
| Mirror/focus |Mirror342 source/68 GLES; focus5,256 source/7,193 GLES |
| Audio works |88,910 source mixer checks,362 historical audio checks,82,091 original START GAME voice checks;907 early weapon sound/position/frequency proofs |
| Leon default firearm/eye details |393,831 source recoil/spread/blink checks,1,576,049 normal-eye/radio-motion checks,1,010 actual shotgun flow checks |
| Native save codec |65 CPU/65,659 exact frozen bytes; fresh unported global managers are zero/preserved basis rather than a complete engine snapshot |

Event GPU checks cover all21 s03 cuts/720 morph observations and all5 s20 cuts/236 morph observations. Maximum tested CPU/GPU skin differences are0.0125204 and0.0136062 source units. Special fixtures cover immutable Esp18 capture/eight layers,09 line width/alpha,0E12-ring depth/history, Filter03 feedback, mask UV/alpha stages and actual Esp08 source power255 indirect A/B warp. GLES RGBA8 substitutes for console RGBA6 EFB storage, so console raster rounding, interpolation and MSAA coverage are not asserted bit-identical. These are host/EGL checks, not Android-device footage or performance measurements.

The final frozen native actual event GLES suite passed1,525,061 checks: all9 r120s00 cuts and15 r120s01 cuts, original EFM models, native Esp18 and independent persistent GPU ownership through cutscene teardown. The radio terminal's actual-asset proof passed1,370 checks across both calls, all11 models, original MDT pages and DSP streams, including source US/UK art/text/pixel equality, START during stream-ready wait and exact810/430 sentinel clocks. Its `IdShimmerTrans` recovered source has an uninitialized local depth divisor; the adapter's defined1500 lower bound is an explicit remaining binary-recovery gap. Eye mode0/1 source proof passed1,576,049 checks through the Leon owner. Integrated native radio/secondary checks are performed by the root build; these component counts do not assert that integration has already passed.

Remaining shared engine dependencies include the full dynamic cAtari/EAT/scenario collision managers, complete camera routing, global RNG scheduling across every owner, full EM10 route obstacle actions and knee/limb reactions, shared voice cooldown/rank route/gait decisions, every unrelated inventory/radio/card state, and source state from unported save managers. The actual r100 flags prohibit ordinary thrown axes and alive weapon fall, so those cannot be invented as a requirement of these particular seven actors. Existing source-faithful bounded paths and the full room inventory support review; they do not support “all Leon actions/all rooms/r100 are100% identical.”

Detailed proofs: `OPENING-M37.md`, `OPENING-EFFECTS-M37.md`, `ROOM-AREA-EFFECTS-M37.md`, `R100-HIT-EFFECTS-M37.md`, `EVENT-RENDERER-M37.md`, `R100-SECONDARY-M37.md`, `R100-DOOR-WINDOW-M37.md`, `R100-EM12-AUDIT-M37.md`, `DOG-TRAPS-M37.md`, `TRAP-EFFECTS-M37.md`, `FOREST-CROWS-M37.md`, `ITEM-EFFECTS-M37.md`, `CAMPAIGN-PATH-FOCUS-M37.md`, `LEON-SWAY-EYES-M37.md`, `STARTUP-AUDIO-M37.md`, and `STREAM-WORK-M37.md`.

## Executable event table

| Table index | Actual filename | Call sites in r100 |
|---:|---|---|
|0|r100s03|Area6 prefetch; first look|
|1|r100s01|No `readEvent(1,…)` in this module|
|2|r100s02|No `readEvent(2,…)` in this module|
|3|r100s20|Area6 prefetch; first death/truck|
|4|r100s30|Posttruck init/death prefetch; first ravine inspection|
|5|r100s41|Pretruck prefetch; officer0 inspection|
|6|r100s42|No `readEvent(6,…)` in this module|
|7|r100s43|Pretruck prefetch; officer1 inspection|
|8|r100s44|Pretruck prefetch; bridge event|
|9|r100s40|Pretruck prefetch; room entry|

The ten-entry table has seven reachable read indices in this module. Dormant table entries are retained as source inventory and are not fabricated as additional playable events. Earlier r120s00/s01 are owned by r120 and are additional to this r100 table.
