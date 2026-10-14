// Test actual production JNI, room preparation/commit and audio lifecycle with
// owned original assets. No fake room music or substitute synthesized sounds.
#define RE4_M27_ENTRY previous_m27_audio_main
#include "test_m27_runtime.cpp"
#undef RE4_M27_ENTRY
#include <future>

namespace m32_audio_runtime {
using namespace m27_test;
struct Pcm { std::vector<jshort> samples = std::vector<jshort>(9600); };
jsize JNICALL arrayLength(JNIEnv *, jarray value) {
  return jsize(reinterpret_cast<Pcm *>(value)->samples.size());
}
jshort *JNICALL shortElements(JNIEnv *, jshortArray value, jboolean *copy) {
  if (copy) *copy = JNI_FALSE;
  return reinterpret_cast<Pcm *>(value)->samples.data();
}
void JNICALL releaseShorts(JNIEnv *, jshortArray, jshort *, jint) {}
void audio(Jni &jni, Pcm &pcm) {
  require(Java_com_ast_re4decomp_NativeBridge_renderAudio(
      &jni.env,nullptr,reinterpret_cast<jshortArray>(&pcm)) == 4800,
      "Production AudioTrack JNI renders its requested interleaved stereo frames");
}
bool audible(const Pcm &pcm) {
  return std::any_of(pcm.samples.begin(),pcm.samples.end(),[](auto s) { return s != 0; });
}
}

int main(int argc,char **argv) {
  using namespace m32_audio_runtime;
  try {
    if (argc != 13) throw std::runtime_error(
        "Usage: m32_audio_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00 WEP02 WEP07 BGM_DIR R111 R113");
    Egl egl; Jni jni; Pcm pcm; const jlong session=320232;
    jni.table.GetArrayLength=arrayLength;
    jni.table.GetShortArrayElements=shortElements;
    jni.table.ReleaseShortArrayElements=releaseShorts;
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[5]));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[6]),string(argv[7]));
    Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,string(argv[8]),string(argv[9]));
    Java_com_ast_re4decomp_NativeBridge_configureMusic(&jni.env,nullptr,string(argv[10]));
    Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
    Java_com_ast_re4decomp_NativeBridge_configureTestProfile(&jni.env,nullptr,1,session);
    const auto create=[&] {
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,
          string(argv[1]),string(argv[2]),string(argv[3]),string(argv[4]),string("r101"),session);
      frozenPixels(jni);
      require(leon && actorGpuReady && walking && !encounter,
              "Production village loads original player/geometry without enemy AI");
    };
    create(); unpause(jni);
    require(roomAmbient && roomAmbient->count()==2 && musicCache.count(0xc000)==1,
            "Production r101 selects its two original ESE emitters and MIDI bank0 sequence0");
    require(streamCache.count(0x8002)==0,
            "Original r101 combat stream2 remains inactive in enemy-free exploration");
    {
      // Reproduce the load/lifecycle contention without hanging the test if
      // a callback regresses to waiting on the mixer gate.
      std::unique_lock<std::mutex> loadingGate(audioGateMutex);
      auto callback = std::async(std::launch::async, [&] {
        Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_FALSE);
        Java_com_ast_re4decomp_NativeBridge_subscreenAudio(&jni.env,nullptr,JNI_FALSE);
        Java_com_ast_re4decomp_NativeBridge_menuSound(&jni.env,nullptr,0x0a);
      });
      const bool returned = callback.wait_for(std::chrono::milliseconds(500)) ==
                            std::future_status::ready;
      loadingGate.unlock();
      callback.get();
      require(returned,"Activity audio callbacks stay responsive while room loading holds the mixer gate");
    }
    Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);
    ticks(jni,2);
    require(roomAmbient->pending()==0 && soundMixer.hasCue(6,0x56) && soundMixer.hasCue(6,0x57),
            "Real world ticks consume the two source r101 ambient sounds from the room bank");
    require(pyreVoice!=0,"Production startup retries the original pyre loop once Activity/world audio is active");
    const auto start=soundMixer.musicFrames(); bool heard=false;
    for (unsigned i=0;i<10;++i) { audio(jni,pcm); heard=heard || audible(pcm); }
    require(heard && soundMixer.musicFrames()==start+48000,
            "Actual AudioTrack JNI plays original village instruments/ambience for exactly one second");
    pause(jni);
    const auto beforeCase=soundMixer.musicFrames();audio(jni,pcm);
    require(!audible(pcm) && soundMixer.musicFrames()==beforeCase,
            "Gameplay pause without a subscreen silences and freezes the production sound engine");
    Java_com_ast_re4decomp_NativeBridge_subscreenAudio(&jni.env,nullptr,JNI_TRUE);
    audio(jni,pcm);
    require(soundMixer.musicFrames()==beforeCase+4800 && audible(pcm),
            "Original case pause preserves the MIDI clock while world simulation remains paused");
    const auto voices=soundMixer.voiceCount();
    Java_com_ast_re4decomp_NativeBridge_menuSound(&jni.env,nullptr,0x0a);
    require(pendingMenuAudio.size()==1 && soundMixer.voiceCount()==voices,
            "UI menu cue is queued without blocking on the mixer");
    {
      const auto beforeCue=soundMixer.musicFrames();
      pcm.samples.resize(2);
      require(Java_com_ast_re4decomp_NativeBridge_renderAudio(
          &jni.env,nullptr,reinterpret_cast<jshortArray>(&pcm))==1,
          "Production audio callback accepts a single stereo frame");
      require(soundMixer.voiceCount()>voices && soundMixer.musicFrames()==beforeCue+1 && pendingMenuAudio.empty(),
              "Production audio callback applies the subscreen gate before playing the original cursor cue");
      pcm.samples.resize(9600);
    }
    audio(jni,pcm);
    const auto beforeBackground=soundMixer.musicFrames();
    Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_FALSE);
    audio(jni,pcm);
    require(!audible(pcm) && soundMixer.musicFrames()==beforeBackground,
            "Activity background silence overrides case music without advancing its samples");
    Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);
    audio(jni,pcm);
    require(soundMixer.musicFrames()==beforeBackground+4800,
            "Activity foreground resumes original case music from its retained sample clock");
    auto *emitterOwner=roomAmbient.get();const auto sequenceOwner=musicCache.at(0xc000);
    const auto beforeEgl=soundMixer.musicFrames();const auto pending=roomAmbient->pending();
    egl.fresh();create();
    require(roomAmbient.get()==emitterOwner && roomAmbient->pending()==pending &&
                musicCache.at(0xc000)==sequenceOwner && soundMixer.musicFrames()==beforeEgl,
            "Production EGL recreation keeps exact source ambience owner and MIDI sample clock");
    Java_com_ast_re4decomp_NativeBridge_subscreenAudio(&jni.env,nullptr,JNI_FALSE);
    unpause(jni);
    {
      auto first=prepareRoom(argv[11],0x111);
      require(first.stream && first.stream->block==0 && first.stream->number==9,
              "Production r111 source automatic stream selects original SHD9 from bio4bgm.sbb");
      first.swapActive();commitRoomAudio(first);
      auto second=prepareRoom(argv[12],0x113);
      require(second.stream==first.stream,
              "Adjacent rooms share the live SHD asset through the weak cache");
      re4::SoundMixer reference;
      for (unsigned slot=0;slot<2;++slot) reference.installSequenceSlot(slot,first.music[slot]);
      reference.installStream(first.stream);reference.setPaused(false);
      std::weak_ptr<const re4::SoundStream> released=first.stream;
      std::vector<std::int16_t> expected(9600);
      // Include the source automatic MIDI slots too: stream9 must continue
      // regardless of whether a room starts or replaces a sampled sequence.
      audio(jni,pcm);reference.render(expected.data(),4800);
      require(pcm.samples==expected,"Production first original stream block matches the independent live mixer");
      second.swapActive();commitRoomAudio(second);
      for (unsigned slot=0;slot<2;++slot) reference.installSequenceSlot(slot,second.music[slot]);
      audio(jni,pcm);reference.render(expected.data(),4800);
      require(pcm.samples==expected,"Source r111-to-r113 commit preserves exact ongoing SHD sample position");
      first.stream.reset();second.stream.reset();reference.clear();soundMixer.installStream(nullptr);
      require(released.expired(),"Weak room-stream cache releases decoded PCM after its last active owner exits");
    }
    Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_FALSE);
    std::cout << "M32 production audio JNI/lifecycle: " << checks << " checks passed\n";
    return 0;
  } catch(const std::exception &e) { std::cerr << e.what() << '\n';return 1; }
}
