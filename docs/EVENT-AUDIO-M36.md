# Event audio and input routing, M36

The bounded r100 event adapter uses the supplied original DSP streams. Actual
`r100s03.evd` contains `Str(1,2)` at cut0/frame0 and `Str(0,0)` at cut14/frame0.
`r100s20.evd` contains `Str(1,9)` at cut0/frame0. These packet fields are not
interchangeable: original `Event::ExePacket_Str` sends block0 through
`SndRoomStrStart`, while block1 is event speech.

M36 keeps block0 in the room `soundMixer` and routes block1 to a separate
`eventMixer`. The audio worker renders both into bounded PCM buffers and adds
them with signed16 saturation. Ending/resetting an event clears its speech;
ending the first encounter preserves the authored room combat BGM0. Cancelling
before that last BGM packet installs the already prepared BGM0 resource. An
already playing identical room stream is retained without restarting its clock.

The shared game/UI audio gate freezes both mixers during background pause.
There are no asset reads in audio rendering or EVD ticks, and UI lifecycle calls
do not wait on a mixer mutex. `streamRenderedFrames()` measures consumed output
PCM frames, freezes on pause, survives natural stream exhaustion and resets on
stream install/clear. It is a validation counter, not an event-clock substitute.

START now reaches the native event cancel branch instead of opening the case.
Its release is still forwarded if the event ended between press and release.
An atomic event-active mailbox prevents UI access to the GL-owned event
pointer. Inventory/Map opens are blocked during an event and rechecked on the
GL owner before opening a subscreen. Android Back and the on-screen back arrow
queue an equivalent complete START press/release only while the event remains
active; the native InputBuffer retains this edge between draw calls.

## Limits

The original event task runs one source frame per `Event::Run` /
`CalNextFrame`; synchronized stream packets wait for stream startup. The source
`EVT_FRAME_RATE=29.97f` is used for stream seek-time calculations. The adapter
currently uses a30Hz event accumulator and retains the normal render-time clamp.
Exact original59.94/29.97 scheduling, hardware stream-start wait and compensation
under very slow rendering are not reproduced. The recorded packet
`synchronized` bit is not a full original sound synchronization implementation.

Adding separately rendered PCM is not a bit-identical port of the original AX
mixing stage: each mixer already limits its own PCM before final summation. Full
original event cancel fades, all remaining event handlers and the general event
runtime are documented with the event adapter separately. Input routing and
persistent BGM do not establish complete event parity.

## Validation

`tools/test_m36_event_audio.cpp` passes12 synthetic independent-stream,
pause/resume, completion, reset and concurrent-control contracts. With the
supplied disc files it passes14 checks, including actual voice1/2 and combat
BGM0/0 resources and retained BGM position after speech stops. The existing
Audio CPU suite passes353 checks with the modified mixer. Device audio/video
synchronization and START/back routing still need validation on the user's
Android device.
