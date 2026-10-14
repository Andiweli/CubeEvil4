# Original Stage 1: reproducible data audit

Preparation inventory; no runtime or campaign completion is implied.

## Totals

- rooms: 29
- aev_door_records: 66
- distinct_aev_directed_pairs: 63
- internal_aev_door_records: 65
- external_aev_targets: ['r200']
- same_room_aev_door_records: 2
- ladder_records: 21
- ita_records: 635
- scr_at_records: 30
- blocks: 5
- campaign_esl_records: 428
- native_door_field_values_checked: 0
- native_start_records_checked: 0
- native_flow_failures: []
- native_static_scene_failures: []

## Rooms and authored AEV targets

| Room | Original editor name | AEV doors | Ladders | ITA | ESL 00 / 01 | AEV targets |
| --- | --- | ---: | ---: | ---: | --- | --- |
| r100 | MORI | 1 | 0 | 24 | 25 / 0 | r101 |
| r101 | MURA | 4 | 1 | 41 | 46 / 0 | r100,r102,r103,r105 |
| r102 | TIKADOU | 2 | 1 | 20 | 1 / 0 | r101,r108 |
| r103 | FARM | 2 | 0 | 28 | 18 / 0 | r101,r106 |
| r104 | KANKIN | 1 | 3 | 27 | 28 / 0 | r107 |
| r105 | SONTYOU HOUSE | 2 | 0 | 27 | 17 / 0 | r101,r107 |
| r106 | HAISON | 2 | 0 | 23 | 16 / 0 | r103,r104 |
| r107 | KANKIN_2 | 2 | 1 | 27 | 25 / 0 | r104,r105 |
| r108 | TOMB | 3 | 0 | 20 | 25 / 0 | r102,r109,r117 |
| r109 | COLOSSEUM | 3 | 0 | 19 | 15 / 0 | r108,r10a,r10e |
| r10a | NUMA | 2 | 0 | 19 | 29 / 0 | r109,r10b |
| r10b | MIZUUMI | 2 | 0 | 17 | 4 / 0 | r10a,r11b |
| r10c | TAKI(night) | 2 | 3 | 19 | 0 / 12 | r10e,r11b |
| r10d | BUKI-SYOUNIN(night) | 1 | 2 | 21 | 0 / 2 | r11b |
| r10e | SUIRO(night) | 5 | 1 | 18 | 6 / 17 | r109,r10c,r10e,r119 |
| r10f | GONDOLA(night) | 4 | 3 | 20 | 0 / 18 | r11d,r11e,r11f,r200 |
| r111 | MURA(night) | 2 | 1 | 25 | 0 / 13 | r112,r113 |
| r112 | TIKADOU(night) | 2 | 1 | 17 | 0 / 1 | r111,r118 |
| r113 | FARM (night) | 2 | 0 | 29 | 0 / 16 | r111,r11c |
| r117 | CHURCH(night) | 2 | 1 | 28 | 0 / 2 | r118 |
| r118 | TOMB(night) | 4 | 0 | 25 | 0 / 12 | r112,r117,r119 |
| r119 | COLOSSEUM(night) | 3 | 0 | 22 | 0 / 2 | r10e,r118,r11a |
| r11a | NUMA(night) | 2 | 0 | 19 | 0 / 11 | r119,r11b |
| r11b | MIZUUMI (night) | 3 | 0 | 19 | 0 / 18 | r10c,r10d,r11a |
| r11c | ROUZYOU(night) | 3 | 0 | 16 | 0 / 10 | r113,r11d,r11e |
| r11d | SEKISYO(night) | 2 | 1 | 25 | 0 / 33 | r10f,r11c |
| r11e | HOKORA(night) | 2 | 0 | 27 | 0 / 2 | r10f,r11c |
| r11f | SONTYOU-BOSS(night) | 1 | 2 | 13 | 0 / 4 | r10f |
| r120 | OPENING | 0 | 0 | 0 | 0 / 0 | scripted entry |

## Scope and interpretation

- AEV graph is authored data, not proof of reachable story states.
- Script jumps, rewritten destinations, events and chapter restarts require code review.
- ESL rows include inactive/event actors and props; row count is not enemy count. Main lists 00/01 only.
- Source references are lexical candidates, not a complete C++ call graph.
- Static start ground checks exclude room-init and moving collision; not walkability acceptance.
- Native scene checks exclude block streaming, scripts and device/GLES visual comparison.

AEV type 0x10 is a ladder, not a room edge. Reciprocal pair presence does not establish that either direction is enabled.
r120 has no AEV; its r120.cpp jump to r100 is part of the scripted opening.
r108 targets r117 in authored data, while r117 targets r118: review day/night scenario switching, not a missing reverse door.
r10e has two self-room transitions with distinct parts; preserve their entry context.
r10f -> r200 is the Stage 2 boundary and needs a transition contract although Stage 2 interiors are outside this scope.

## Native diagnostics

Native availability comes from the supplied current room_flow_tool, not an audit allowlist.
Ground samples use only base SAT, radius 300, height 1600, search +/-250.01; scene counts use default static placement.

