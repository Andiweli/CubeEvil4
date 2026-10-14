# M36: lazy original motion metadata review

The frozen source reference is `9b76ff003081d6ece8b838453fb8310743a972c7`. This review concerns preparation and motion-state preservation, not new gameplay dispatch.

`AnimationRig::cycle` now calculates original `MotionSetCore` cycle metadata on demand. `MotionSetCore` and `IKInit` write part flags, Hermite history and interpolation work even when their `MOTION_INFO` argument is separate. The new code therefore copies and relinks a private model/parts list before obtaining metadata. Its parent pointers refer to the private model and parts. The live primary/secondary work, attached camera, solved matrices and immutable clip ownership are separate. No production correction was needed after review.

`m36_loading_check PL00 WEP01 WEP02 WEP07 EM12` compares two independently constructed players with every supported firearm installed. The control obtains every cycle before starting motion, matching the prior eager cache. The other player first requests every cycle midway through a live original action, then repeats those queries after more motion. Five cases cover draw Hokan, firearm three-way blend, additive knife blend, original EM12 kick attached-camera curves and looping gait. One case also appends an original FCV/SEQ during a running blend.

Future root extraction, solved part matrices, FCV/SEQ frames/state, attached-camera fields and original motion-sound events remain equal for64 native advances per case. This guards hidden live history as well as the immediate copied pose. The supplied-data result is2594 checks,922 cycle queries across all92 installed clips (93 after the one append), and64 live original attached-camera frames. The six preparation fingerprints remain`8956647433431119642`, matching the recorded prior preparation fingerprint.

Validation runs outside the timed preparation region. The tool still reports the six preparation durations so the application owner can compare equivalent builds separately. Faster preparation does not establish complete room/gameplay loading parity, and this metadata change does not port missing player states, scenario scheduling or original item/menu ownership.
