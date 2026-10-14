# M37 original EM2A effects

The first-level dry-land trap adapter loads the actual EM2A EFF from the enemy
DRS and registers textures in original CORE-first ownership order. All source
records stay byte-for-byte immutable. The original EST banks are not activated
globally during room load.

| Original EST | Source caller / role | CPU and renderer owner |
| --- | --- | --- |
| 0 / 4 | cEm2a dry-land blast types 1 / 2 | Esp00/0b sprites, Esp09 lines, Esp0e projected lenses, Esp10 SAT decals, Esp11 CORE light, Esp46 Filter03 request; real cObj05 fragments |
| 1 | Leon caught by bear trap | Part23 attachments, source Tool20 world spawn, Esp07 cached SAT floor and stopped blood, Esp10 floor decals |
| 2 | Bear trap closes | Authored Esp00 and real cObj05 model14 |
| 3 / 5 | Armed tripwire type 1 / 2 | Original finite and persistent Esp0b generators, tied to the original actor lifecycle |
| 9 | Detached original impact smoke / line | Original explicit world placement and Esp09 history |

`BuildTrapWorldEffects` exposes those seven banks. The source water explosion
EST6 requires its separate Esp40 water path and is not represented as a dry
explosion. Ashley-specific EST7/8 are outside the Leon first-level path.

`triggerAttached` requires actual current posed world part matrices. Parts0/2
belong to the authored trap posts; Part23 belongs to Leon. `updateInstanceParts`
refreshes those matrices before each original 30Hz move. `stopInstance` deletes
persistent effects when the original owner dies, with duplicate event serials
still retained so the armed sequence cannot accidentally restart.

Source Tool20 uses owner rotation and the current joint translation, then
consumes three additional world R_pos random draws. It retains the initial
three R_pos draws too. Esp10 detaches at SetFreeWork before its source SAT
probe. Esp0e's attached Release_time0 is overridden to FF, as in the original.

Esp09 keeps four newest-first world positions for these Work8=0 records. Its
GX line width is the original truncated u16 size times0.03, attenuated after
the first source frame beyond5000 units. Rendering narrows this to the native
GX byte width and maps sixth-pixel units to the Android viewport. Tail alpha
uses the original truncated byte step and subtraction.

Esp07's actual Leon bank has HitType0, EstCall0 and SeType0. It caches its first
600-up /100000-down SAT floor result, snaps by half the original sprite height,
reflects speed by Vec0 times0.1, and freezes speed/acceleration below10 units.
Its source Life_max0 becomes128. Work8's CORE28 value does not spawn an extra
effect because EstCall is zero.

The nine dry-bank FE records are routed to the independent real cObj05 model
backend. `drainFragmentSpawns` publishes each original record index, actual
posed parent matrix, original source event identity and exact pre-spawn seed
at its original activation or generator time. EfmSetObj05 consumes thirteen
shared sequence-seed draws; subsequent Kaboom randomness uses the separate
seed copied before those draws. This preserves the original mixed sprite /
model random stream instead of starting all fragments from one guessed seed.

The source oracle `tools/test_m37_trap_effects.cpp` runs all seven actual banks
against the actual r100 SAT. It checks mixed-EST seed consumption, FE callbacks,
line history and alpha bytes, original lens parameters, caught blood floor
handling, persistent owner deletion and parented checkpoint restoration.
GPU depth occlusion and Android-device appearance need their separate checks.

The actual-asset CPU run passed62,246 checks:2,340 line requests,32 lens requests,7,628 caught-blood observations and10 original fragment spawns (nine authored dry-bank FE records, with the bear's generator emitting twice).
