# M37: original r100 firearm surface impacts

`R100RoomHitEffects` ports the actual ordinary gun surface path from `EatGetEffectType`, `AtEffInfo::getWepEff`, `EspSetEatEffect`, `PlSetWepTarget`, `esp05.cpp` and the detached `esp40.cpp` profile. It retains the complete three original r100 effect tables and every pair. Ordinary weapons1…12/0F/11/21 use **effGun**, the fifth pair; the first pair is not the firearm selection.

| EAT effect type | Original ordinary gun result |
|---|---|
|0|CORE1F; source FLR puddle with normal.y>.9 instead CORE11 + sound2/C|
|1|CORE1F + CORE20|
|2, WATER|ROOM20 + sound2/B; source flag1 changes normal orientation|
|4, ROOM0|ROOM0E; no extra sound|
|5, ROOM1|EFF_NONE; flag2 also drives source DontFire|
|3 / unregistered6/7|Explicit diagnostic; no guessed sprite|

The immutable bank contains36 actual300-byte records across CORE1F/20/11 and ROOM20/0E. It registers original ANM/TPL textures CORE-first. ROOM0E has three Esp05 records and seven Esp00 records. Esp05 uses its original amplitude/speed/phase initialization, all global Rnd draws even when spread is zero, source flutter sine/cosine updates, CommonMove and ANM. The real generator's global jitter precedes each SetFreeWork, so the initializer borrows the live Environment visual RNG through a header-only accessor; no object-layout or callback ABI changes were made. All controller moves precede all particle moves across these instances.

ROOM20 has seven Esp40 records, all with world parentFF. The source detached branch queries actual `GetWaterHeight` once at creation, sets height+authored Pos.y and subsequently uses normal CommonMove/ANM. It does not continuously stick a detached particle to the water or invent an attached parent. Source `PlSetWepTarget` suppresses an EAT impact at/below an available water plane. The separate `wepSetWaterShot` cross-plane wave push is not claimed by this adapter.

The bridge uses the actual nearest EAT point/normal and gun mask0x400000, and suppresses that normal when a registered actor/object/window/door wins the source target selection. All thirteen shotgun ray candidates are captured before any damage changes a pane or object. Each ray has its own stable serial. The original FSE puddle test is supplied separately; all96 r100 floor records use group0. Source sounds are drained to the room mixer. GPU textures have an independent roomHit owner, including EGL recreation. Impacts use Core_flg0 and therefore pause/hide under source suspension; permanent SST effects use their separate source0x4001 policy.

Legitimate explosive weapon selections are retained and returned as explicit owner/EST requests with a warning. Their Esp45/4A/FE/global camera dependencies are not substituted with the gun banks, and cannot crash ordinary play through a strict unsupported-builder exception. The adapter owns a bounded512-particle presentation budget; the complete original cross-manager pool stealing and global RNG schedule remain outside this proof.

`python3 tools/test_r100_surface_source.py` proves byte-identical copies of all three AtEffInfo tables and five original function bodies. `m37_r100_surface_effects_check R100.DAT CORE.DAS` passed74,019 checks: all256 weapon IDs,65,536 attribute words, all36 immutable records, strict puddle orientation,120 exact source flutter observations/global RNG states,530 finite original quads, seven detached water sprites, original Esp40 initialization, natural slow mark fading, duplicate serial suppression, underwater-hit suppression and nonthrowing unsupported explosive requests. This is an actual-asset CPU proof. Final native/GLES/Android integration is verified by the root build separately; it is not Android-device footage.
