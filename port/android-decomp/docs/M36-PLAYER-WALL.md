# M36: original posed scenery body

Reference source is frozen at `9b76ff003081d6ece8b838453fb8310743a972c7`.

The supplied `leon-verschwindet-in-wand.png` shows Leon's ordinary WEP02 stance next to the upper shotgun-house wall in `r101`. This is real mesh/scenery overlap. The renderer already uses ordinary opaque depth-tested arms; hiding the overlap by changing depth would leave the physical placement incorrect.

## Original body and update order

`game/player.cpp` initializes the player with `atari.init(0,-200,0,400,200,400,800,1,0x1000,10)`. The scenery radius is400, distinct from the character-contact radius200. `game/atariInfo.cpp::getSpeedVector` obtains X/Z from posed part0, with the model-rotated offset, but deliberately sets Y to actorY+radius. The negative200 offset therefore does not lower this scenery sphere.

`cPlayer::move` resolves scenery after action motion, body waist twist and `partsWorldCalc`, including stationary idle/turn/draw/aim poses. The existing native adapter checked a diagnostic radius300 at actor feet only during root movement. It omitted the original posed400 body after stationary animation.

Ordinary movement, draw, aim, fire, reload, holster, knife and kick now resolve the original posed sphere after each native30Hz tick. Returned sphere corrections move actor feet and the rendered model together. The floor query retains the original600-above/100000-below interval and strict1000 snap limit. Render interpolation carries the contact correction in both placement endpoints so a display-only half tick does not move the mesh back through the wall.

The selected source `At_poly_sphere_ck2` face/front/edge logic is transferred into `SatCollision::resolveScenerySphere`, with separate face then edge passes, authored cancelled-edge bits and the source0.99 edge radius. Original SAT block wall-reference order is retained instead of using BVH traversal order. `collision_query.cpp` uses strict floating-point contraction settings. Initial/final swept-line queries continue to use the existing checked native SAT query.

No source ordinary wall-lowering/re-aiming state was found in the relevant handgun/shotgun/knife routines. Original FCVs, Hokan, weapon ownership and attack timings are retained. Original gun/knife tips can extend beyond their player's body sphere when striking or facing a wall; this change does not substitute a large mesh-shaped capsule for the original body.

## Actual supplied-data validation

`m36_player_wall_check PL00 WEP01 WEP02 WEP07 EM12 R101` uses the actual original shotgun pickup at `(1085.6,4703.6,-17573.3)` and upper floorY3200. Its neighbouring west house wall is actual SAT polygon583, normal approximately`(+0.999995,0,-0.0032536)`.

The accepted old300-body position leaves the original idle mesh53.15units through this wall. The original posed400 correction moves actor90units and leaves36.85units of actual visible mesh clearance. This is checked with actual original skins and bone/bind matrices, not a proxy box. All1080 native pose ticks across WEP01/WEP02/WEP07 idle, turn, draw, aim, single shot/pump, reload, holster and knife draw/slash/sheath retain bit-identical FCV part matrices and frames against an independent control replay.27 captures of the production GLES shader, including a display-only half tick, agree with independently skinned world vertices within0.001194units and preserve `GL_DEPTH_TEST`/`GL_LEQUAL`.

`WalkProbe::setActorBlockers` also supports the enemy group's whole live upright collection. Every movement/slide segment tests all contacts and stops at the nearest actor. Reversed registry order, removal and the existing zero/one-actor API are checked on the actual house floor. A second actual-house check proves that an actor missed by the initial diagonal segment still blocks the path after the west wall deflects it.

The same tool checks fresh source firepower-level metadata0, accepted values0..6, rejection of invalid weapon/level values, shot ownership and preservation through room reconstruction. This stores the selected profiles' power nibble; it does not implement original tuning purchases or the complete item-manager attributes.

## Scope

The diagnostic movement sweep, dynamic object/character collision, SAT BVH candidate selection and line query remain native adapters. This is the relevant original posed-body correction, not a transfer of the entire `cAtari` manager. Scripted traversal, paired grabs and blow adapters retain their existing suspension/placement contracts; other damage and event-specific body resizing are not newly implemented. The complete M35 action inventory remains applicable and is not a claim of complete original gameplay parity.
