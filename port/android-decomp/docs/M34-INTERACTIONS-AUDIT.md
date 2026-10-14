# M34 original interaction and destruction audit

This audit covers the complete GameCube registries, rather than treating the two test rooms as the game. It inventories all 104 `Et00..Et67` initializers, all 64 `cObjMgr` IDs, all 21 GameCube scenario-area handlers, and every ETS/ITA/AEV record in the 127 room archives selected by `app/src/main/assets/rooms.tsv` from the supplied Disc1/Disc2 files. The machine-readable companion `M34-INTERACTIONS-AUDIT.json` records the original initializer, class/factory, per-ID counts, per-room counts and decoded archive SHA256. These are source/data coverage figures; they do not certify that every campaign event or animation now plays identically.

## Data inventory and native profile construction

The 127 selected rooms contain 1504 ETS placements across 99 etc IDs and 1601 ITA records across 132 authored item/system IDs. ITA waiting types are 1003 free records (type0), 135 enemy-linked records (type 1), and 463 etc-linked records (type2). The original registrations include five etc IDs with no placement in this selected room set; the audit still includes them.

`tools/test_m34_interaction_profiles.cpp` constructs the supported native interaction, container and pickup owners for every decoded room and generates each container's actual `RoomItemLayer`. The final scan reports no constructor or item-layer exceptions in any of the 127 rooms. All 127 item-layer checks preserve the owner's snapshot, including source RNG and pickup transaction state. `M34-INTERACTIONS-RUNTIME.json` records each result, including rooms with zero eligible objects.

| Constructible subset | Eligible placements | Rooms containing that subset |
|---|---:|---:|
| cEmBox types 0/1/3/5 |410|75 rooms contain boxes and/or explosive barrels|
| Non-rolling explosive barrels |62|Included in the75 above|
| Prerequisite-free waiting0 pickups |918|115|
| ET00/07/25 window profiles |68|29 rooms contain windows and/or ET03/0D doors|
| ET03/0D interior door profiles |26|Included in the29 above|

Eligible means the original data can construct the current native subset. It does not mean that all these placements have received a visual play-through, or that their room initializer, lock event, enemy script or acquisition cinematic is implemented. Nine original empty ITM packs now correctly accept an ID table with count 0 and null BIN/TPL tables; `cItmSys::DataLoad` never dereferences those tables because its loop has zero iterations. The ET07 profile in r11d no longer disables its owner for absent intact glass-vault FCVs: shared broken-window short-fence motion remains available, and unsupported glass/drop directions produce no offer.

## Complete etc-class mapping

The following lists partition all 104 original etc IDs. Every ID is also a separate entry in the JSON, including IDs that the port currently only renders.

| Original class | Original etc IDs(hex) | Selected placements | Current live behavior and gap |
|---|---|---:|---|
|cEmWindow|00,07,1d,25,29,2c,35,36,44,48,4a,50,51,52,53,54,55,56,57,58,5a,5b,5c,5d,5e,5f,60,64,65|364|Shared ET00/07/25 source collision, hit rules, broken state and available vault assets. The other 26 model/damage/field profiles and campaign overrides remain unported.|
|cEmBox|01,02,11,1e,1f,2e,2f|522|Shared01/02/11/1e(types 0/1/3/5), source attack shapes, one attack serial per object, debris and eligible ITA contents. Nest1f and pottery2e/2f types, their animations and replacement models remain unported.|
|cEmDoor|03,09,0d,13,16,17,18,20,21,22,23,24,27,2a,2b,31,32,33,34,37,39,3b,3d,3e,3f,40,41,45,46,47,49,4d,4e,59,62,63|160|Shared03/0d base opening/kicking/closing and checked r101 lock. Other door types, pane-by-pane destruction, arbitrary script locks, double leaves and event overrides remain unported.|
|cEmRack|04,05,15,30,66|23|The three original r101 furniture slots 13/15/17 retain their explicit source ranges and push FCVs. Other movable placements require their original room callback limits; they are not assigned guessed ranges.|
|cObjLadder|06,08|31|Checked r101/r400 climb, descend, tip, reset, body collision and prop FCV/SEQ events. Other rooms can reposition or initialize downed ladders in their callbacks; those states remain unported.|
|cEmTorch|0a,0b,0c,10,14,19,1c,38|263|Source geometry/environment effects may render. The complete hanging/moving/shot reaction and room event object runtime remains unported.|
|cEmSwitch|0e|18|Switch object action/event runtime remains unported.|
|cEmBarred|0f,1b,26,28,3a,43,4b,4c,4f,61,67|28|Script-driven barred gates and their event transitions remain unported.|
|cEmBarrel|12,2d,3c|62|Checked non-rolling explosive profiles, source hit shapes, two-tick delayed blast, chain delays and occlusion. Rolling/moving/event-controlled states remain unported.|
|cEmItem|1a|30|Treasure-chest lid/opening/acquisition runtime remains unported.|
|cEmBar|42|3|Lever object action/event runtime remains unported.|

## Window defects and retained original damage

`pl_class.cpp::windowCheck` uses a point 1000 units forward at feetY+400. `ChkWindow` tests that endpoint against the original field rectangle and then requires a crossing of the window plane. The port now follows that test, rather than adding a facing cone or testing only the ray-plane intersection. The field survives a pane breaking.

For an already broken window, `cPlayer::actionSelect` selects `fanceOn` for same-height directions 0/1. These use the original short PL_ARC55/56 motion pair with r3=5 and no `fanceAdjust`. Intact panes and direction 2 use `windowOn` and the original ETM vault clips. Re-entry now chooses that source branch. Genuine landing floor and bounded restored body contact are checked after the original airborne root path; the r101/r400 upstairs incoming path no longer disappears because its source endpoint touches scenery slightly. `obj13::LadderNearCk` also reserves window transitions while a standing ladder top lies within 2000 units; falling/fallen ladders release that reservation.

Two pistol shots for ordinary ET00 house glass are intentional original behavior: `WindowData[0]` starts at 1000 HP with hpType 1; group 0 hits subtract 500. ET25 cupboard glass has hpType 0 and breaks on one hit. Shotgun(group 1) and knife(group 2) break ordinary house glass at once. M34 preserves these rules instead of changing all panes to one pistol hit.

## Items, acquisition and destruction

Generic compatible container/pickup owners are no longer limited to r101/r400. An unsupported parent, callback, country/character branch, item prerequisite or mode-specific record is suppressed individually; it does not disable every compatible container in the same room. Unbound containers keep `emBoxInit`'s original `itemNo=-1`: they break and produce debris without an invented drop.

Container items now use the complete `OriginalItemInfo` table instead of a short hard-coded item whitelist. Pickup quantities follow `cItemMgr::get`: weapon type 1 and attachment type 9 create one item; other new entries use the source default and `maxNum` clamp. Money IDs 78/79/71 convert before stack clamping, including the correct 79 default of 30 units. Mercenaries time/score IDs 73/75 require their mode manager and stay suppressed. The Leon special ammo branches 1003/1004/1005 remain the original 15 shotgun/10 shotgun/20 handgun rounds. Random 1001/1002 retain the checked source recurrence and available Leon supply context, but the port does not yet own the original whole-campaign RNG stream or every weapon/character supply branch.

Missing ITM model variants are valid original data, not failed TPL aliases. `SceAtCreateItemAt` checks `ItemGetBinTplAddr` and omits only `setItemObj` when no model is registered; the item area and `sceAtItemEffSet` remain. The native item layer now follows that model behavior: it skips only a missing variant's geometry, retaining its owner, stable draw indices for present variants, original RNG choices and pickup transaction. The complete 127-room layer scan finds 363 bound-item owners, 1117 candidate variants with actual room models and 128 candidate variants without a model across 23 rooms. These are preloaded theoretical choices, not 128 currently spawned items. For example r331 and r223 lack random-model IDs 01/02/05/0e/19; r11d lacks 05. The original model-less item's glow/effect backend remains explicitly unported, so geometry preservation does not claim its original visual cue is complete.

The 918 eligible free records retain original area, facing, EAT line visibility and taken-flag handling. Bound container pickups now also check the authoritative `ScenarioState` flags during rendering, target selection and final acquisition: global `item_flg` takes precedence over room-local `auto_item_flg`. Successful acquisition commits only the source taken flag. The original found flags belong to separate shoot-down/falling branches; ordinary container and free-item acquisition preserves them, including preexisting found bits. A restored old room snapshot cannot bypass an item already taken in another room. Empty containers remain destructible.

The 39 actual-data flag checks cover r103 ITA9/ETS5 and r113 ITA0/ETS5 sharing global flag7/item5e, plus r10d flag24/itemc7, r11d flag21/itemc8, and r306 flag55/item59. Each still breaks normally while a taken treasure cannot be acquired twice. Synthetic regressions additionally prove local auto flags remain independent between rooms, global precedence leaves local flags unchanged, cancelled acquisition changes no flags, and restored active drops remain hidden by authoritative global flags.

The full acquisition message/task sequence, every enemy-linked drop, falling/shoot-down collectible, item-parent object, room event mutation and original save-item reservation manager remains incomplete. The attached case subsystem separately owns capacity, stacking, placement and menu behavior; constructor eligibility must not be equated with completing those original game tasks.

All64 original object manager IDs and their placement-new classes or registered `ObjInitFunc` factories are enumerated in the JSON. The current live subset includes selected weapon objects, checked ladder objects, selected ITA item models/transactions and Efm05 destruction. The original projectile/grenade/bow/missile objects, church bell, gatling, event objects, raft, chains, pillars, parasitic objects, gondola, Salazar statue, helicopter missiles, trolley and bulldozer managers are not all implemented. Likewise, all 21 `sceAtFunc_tbl` handlers are listed: generic event execution, arbitrary flags/messages, save, damage/script areas, stoop, keys/use/hide/item-parent and position-jump task paths still require their original runtime. Rendering a model or parsing its area does not implement those handlers.

## Verification and reproduction

`object_actions_tests` passes source container hit shapes, serials, EAT exceptions, delayed/chained blasts, snapshot restore, individual prerequisite suppression, empty-box destruction, complete-table quantity defaults, attachment quantities, uncapped money conversion and authoritative global/local taken flags. `room_items_tests` passes model-table bounds and the original empty-pack variant. Actual r101/r400 `room_items_check` additionally passes mixed missing/present model fixtures, stable draw-owner indices and fully model-less owners without invented geometry or changed RNG. `context_actions_tests` passes original fence permissions and motion placement contracts. The all-room profile tool also passes all 127 actual item-layer generation checks and the 39 flagged-item regressions described above.

`m33_context_actions_check` passes 9070 checks on actual r101/r400 SAT, original player/weapon FCVs and all checked object-ladder clips. It includes 24 repeated incoming/outgoing broken-window traversals, independent PL_ARC55/56 pose/root comparisons, upstairs/ground landings, no repeated glass burst, and standing/falling ladder reservation checks. The production JNI/GLES fixture passes 5696 checks after the ladder reservation and authoritative flag wiring. Its genuine r400 cold start verifies the source initially broken window0 has0HP, ETC bit1, no damage target, no invisible pane SAT and no repeated glass fragments. Physical A then tips nearby authored ladders before entering their reserved windows and exercises repeated window passages, owner state, rendering and effect serials. The final integrated build result is recorded in the M34 validation report.

Rebuild the host targets, then regenerate the two manifests with:

```sh
python3 tools/audit_m34_interactions.py DECODED_ROOMS docs/M34-INTERACTIONS-AUDIT.json
m34_interaction_profiles_check DECODED_ROOMS PL00 > docs/M34-INTERACTIONS-RUNTIME.json
m33_context_actions_check R101 R400 PL00 WEP01
```

All paths above refer to original user-supplied assets or local test executables. The repository audit includes counts/hashes/source references and does not copy original game asset content into the report.
