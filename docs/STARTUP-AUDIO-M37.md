# M37: complete START GAME voice

The original `src/game/title.cpp` starts the English announcement with
`SndCall(6, 2)` and a 90-frame fade in `titleMenuMain`. In `titleExit`, lines
1508–1511 wait for `SndEndCheck(w->se_id)` before releasing the title and chaining
into `GameTask`. These are two separate completion conditions.

M36 switched to the opening decoder as soon as the three-second fade finished.
The supplied NTSC Disc 1 announcement contains two chained SIT layers and lasts
226,080 output frames at 48 kHz: **4.710 seconds**. The old fixed fade therefore
cut off 1.710 seconds of the source cue.

M37 keeps the title audio owner active until the fade and that voice both finish.
`SoundMixer::voiceActive(handle)` tracks the shared handle across all chained
layers; ordinary pause retains the original sample positions. A native atomic
completion mailbox also covers a queued announcement that the audio worker has
not consumed yet. Pending title cues survive a lifecycle pause, and title bank
reconfiguration for the same imported disc preserves the current voice.

Native completion is followed by a second, Android-specific check. `GameAudio`
records the final accepted stereo PCM frame and waits until AudioTrack's playback
head reaches that frame. Consequently, starting the movie cannot flush the
announcement's buffered tail. The wait uses actual accepted sample counts and
the hardware playback head, with unsigned 32-bit wrap handling; it adds no guessed
timeout or fixed sound delay. A protected voice buffer is retained across a
lifecycle pause. The consumed logo cue flag is saved so Activity recreation
cannot replace a pending announcement with the earlier title-logo cue.

## Validation

- `tools/test_m37_startup_audio.cpp`, using the supplied original
  `ss/Cmn/title.snd`: **82,091 checks PASS**; two chained layers; 226,080 output
  frames; voice still active at the original 90-frame fade boundary; pause emits
  silence without consuming the announcement; completion includes the final
  source PCM sample.
- `tests/audio_tests.cpp`: **362 checks PASS**, including paused handles,
  natural completion, explicit stop, and a two-layer fixture whose first layer
  finishes before the second.
- `StartupFlowTests`: **19 checks PASS**; `StartupVoiceTests`: **14 checks PASS**,
  including independent fade/voice completion, buffered tail, final playback
  frame, pause, partial stereo writes, playback-head wrap, and flush reset.
- `GameAudio`, `AudioPlaybackFence`, and `NativeBridge` compile against the
  Android API 35 framework with `javac -Xlint:all` and no warnings.

AudioTrack device scheduling and the announcement-to-opening transition still
require the normal Android device test. The automated output checks validate
frame accounting and the real native cue, not a physical speaker recording.
