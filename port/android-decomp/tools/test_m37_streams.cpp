// Independent Snd_str 8.8-volume/5ms oracle and concurrent PCM ownership.
#include "sound_mixer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks;
void require(bool value,const char *why) {++checks;if(!value)throw std::runtime_error(why);}
std::shared_ptr<const SoundStream> asset(unsigned block,unsigned number,unsigned volume=127) {
  auto stream=std::make_shared<SoundStream>();stream->block=block;stream->number=number;
  stream->rate=48000;stream->volume=volume;stream->pan=64;stream->stereo=stream->looping=true;
  stream->left.assign(64,std::int16_t(100+number*11));stream->right.assign(64,std::int16_t(-200-int(number)*7));
  stream->loopLeft=stream->left;stream->loopRight=stream->right;return stream;
}
void render(SoundMixer &mixer,unsigned frames) {
  std::vector<std::int16_t> pcm(frames*2);mixer.render(pcm.data(),frames);
}
struct FadeOracle {
  int now{},end{},step{};bool fading=true,playing=true;
  FadeOracle(unsigned start,unsigned target,unsigned time):now(int(start)<<8),end(int(target)<<8) {
    const int difference=end-now;step=difference/int(time);
    if(!step)step=difference>0?1:-1;
  }
  void tick() {
    if(!playing || !fading)return;
    if(now==end) {fading=false;if(!now)playing=false;}
    else {
      int next=now+step;
      if(step>0) {if(next>end)next=end;}
      else if(next<end)next=end;
      now=next;
    }
  }
};
}
int main(int argc,char **argv) {
  try {
    const auto room=asset(0,0),a=asset(1,4),b=asset(1,5),c=asset(1,6),extra=asset(1,7);
    SoundMixer prepared;prepared.installStreamAssets({a});prepared.setPaused(false);
    const auto preparedHandle=prepared.requestStream(1,4,1);
    require(preparedHandle && prepared.streamActive(1,4) && prepared.streamReady(1,4) && !prepared.streamPlaying(1,4),
        "Source prepare owns a ready work slot without starting its AX voices");
    std::vector<std::int16_t> preparedPcm(1600);prepared.render(preparedPcm.data(),800);
    require(prepared.streamRenderedFrames(1,4)==0 && std::all_of(preparedPcm.begin(),preparedPcm.end(),[](auto value){return !value;}),
        "Radio waitReady advances neither spoken PCM nor its fade clock");
    require(prepared.requestStream(1,4,2)==preparedHandle && prepared.streamPlaying(1,4),
        "Separate source request2 starts the prepared speech owner");
    prepared.render(preparedPcm.data(),800);
    require(prepared.streamRenderedFrames(1,4)==800 && std::any_of(preparedPcm.begin(),preparedPcm.end(),[](auto value){return value!=0;}),
        "Prepared speech begins at its original first sample when play is requested");
    require(prepared.requestStream(1,4,2)==preparedHandle && prepared.streamRenderedFrames(1,4)==0,
        "Original play request resets block playback while retaining its allocated id");
    SoundMixer mixer;mixer.installStreamAssets({room,a,b,c,extra},true);mixer.setPaused(false);
    require(!mixer.hasMusic(),"Registering resident streams does not auto-start combat/event PCM");
    const auto roomHandle=mixer.requestStream(0,0,0x80000003);
    require(roomHandle && mixer.requestStream(1,4,3) && mixer.requestStream(1,5,3) && mixer.requestStream(1,6,3),
        "Four original stream work slots coexist");
    require(!mixer.requestStream(1,7,3),"Fifth stream preserves original four-work-slot limit");
    render(mixer,480);std::vector<std::int16_t> pcm(512*2);mixer.render(pcm.data(),512);
    const auto expectedLeft=std::lround(.7*(room->left[0]+a->left[0]+b->left[0]+c->left[0]));
    const auto expectedRight=std::lround(.7*(room->right[0]+a->right[0]+b->right[0]+c->right[0]));
    for(unsigned i=0;i<512;++i) {
      require(pcm[i*2]==expectedLeft && pcm[i*2+1]==expectedRight,"Concurrent stereo streams sum original independent PCM");
    }
    const auto oldFrames=mixer.streamRenderedFrames(0,0);
    require(mixer.requestStream(0,0,0x80000003)==roomHandle && mixer.streamRenderedFrames(0,0)==oldFrames,
        "Repeated signed room start retains the actual playback phase");
    mixer.requestStream(1,4,4,400,0);render(mixer,1000);
    const auto fadingVolume=mixer.streamVolumeFixed(1,4);const auto frozen=mixer.streamRenderedFrames(1,4);
    mixer.setPaused(true);mixer.render(pcm.data(),512);
    require(std::all_of(pcm.begin(),pcm.end(),[](auto sample){return !sample;}) &&
        mixer.streamVolumeFixed(1,4)==fadingVolume && mixer.streamRenderedFrames(1,4)==frozen,
        "Lifecycle pause freezes addressed PCM and5ms fade clocks");
    mixer.setPaused(false);mixer.requestStream(0,0,4,600,0);render(mixer,480);
    require(mixer.streamStopping(0,0),"Source room fade records stop_req");
    const auto beforeResume=mixer.streamRenderedFrames(0,0);
    mixer.requestStream(0,0,4,600,room->volume);render(mixer,480);
    require(!mixer.streamStopping(0,0) && mixer.streamRenderedFrames(0,0)>beforeResume,
        "Source room start while fading restores volume without rewinding music");
    // Every output chunk boundary is independent of the source audio callback.
    // Compare complete original fixed-point fades, including quantization and
    // the extra callback between hitting zero and releasing the work slot.
    for(unsigned start:{0u,1u,5u,63u,64u,126u,127u})
      for(unsigned target:{0u,1u,5u,63u,64u,126u,127u})for(unsigned time:{1u,2u,50u,400u,600u}) {
        SoundMixer test;const auto stream=asset(0,2,start);test.installStreamAssets({stream});test.setPaused(false);
        require(test.requestStream(0,2,0x80000003)!=0,"Original test stream begins");render(test,480);
        test.requestStream(0,2,4,time,target);FadeOracle oracle(start,target,time);
        for(unsigned tick=0;oracle.fading && tick<32768;++tick) {
          oracle.tick();render(test,240);
          require(test.streamVolumeFixed(0,2)==unsigned(oracle.now),"Original signed8.8 fade step/clamp differs");
          require(test.streamActive(0,2)==oracle.playing,"Original fade-to-zero work release differs");
        }
        require(!oracle.fading,"Original fixed-point fade reaches its target within bounded steps");
      }
    // Stop command8 always uses50 callbacks, independently of its requested
    // time parameter. Other addressed streams keep their playback clocks.
    SoundMixer stop;stop.installStreamAssets({a,b});stop.setPaused(false);
    stop.requestStream(1,4,3);stop.requestStream(1,5,3);render(stop,480);
    stop.requestStream(1,4,8,999);FadeOracle stopOracle(127,0,50);
    unsigned stopTicks=0;
    while(stopOracle.playing) {
      stopOracle.tick();render(stop,240);++stopTicks;
      require(stop.streamVolumeFixed(1,4)==unsigned(stopOracle.now) && stop.streamActive(1,4)==stopOracle.playing,
          "Original command8 uses the native50-step fade and delayed close");
    }
    require(stopTicks==52 && stop.streamActive(1,5) && stop.streamRenderedFrames(1,5)==480+stopTicks*240,
        "Ending one source stream leaves its neighbor and native PCM phase intact");
    // Existing event/title install API owns only its selected stream. It must
    // keep the source speech running when room/event residents are installed.
    SoundMixer legacy;legacy.setPaused(false);legacy.installStream(a);render(legacy,800);
    const auto speechFrames=legacy.streamRenderedFrames();legacy.installStreamAssets({room,b});
    legacy.requestStream(0,0,0x80000003);legacy.requestStream(1,5,3);
    legacy.installStream(a);require(legacy.streamRenderedFrames()==speechFrames,"Legacy spoken stream identity survives independent channels");
    render(legacy,700);require(legacy.streamRenderedFrames()==speechFrames+700 && legacy.streamActive(0,0) && legacy.streamActive(1,5),
        "Legacy speech and source addressed streams advance independently");
    legacy.installStream(nullptr);require(!legacy.streamActive(1,4) && legacy.streamActive(0,0) && legacy.streamActive(1,5),
        "Clearing the selected legacy stream preserves independent room/event streams");
    legacy.clear();require(!legacy.hasMusic() && !legacy.requestStream(0,0,0x80000003),"Full mixer clear removes stream slots and resident assets");
    if(argc==2) {
      const auto base=std::filesystem::path(argv[1])/"bgm";
      const auto selection=ReadRoomMusicSelection((base/"bgmtbl.dat").string(),0x100);
      const unsigned code=selection.streams[0]&0xffff;
      require(code!=0xffff && (code&0x8000),"Actual r100 table has its original resident combat stream");
      std::vector<std::shared_ptr<const SoundStream>> originals;
      originals.push_back(SoundStream::Load((base/"bio4str.hed").string(),(base/"bio4bgm.sbb").string(),0,code&255));
      for(unsigned number:{4u,5u,6u})originals.push_back(SoundStream::Load(
          (base/"bio4str.hed").string(),(base/"bio4evt.sbb").string(),1,number));
      SoundMixer actual;actual.installStreamAssets(originals,true);actual.setPaused(false);
      require(actual.requestStream(0,code&255,0x80000003),"Actual room combat stream starts from its table number");
      for(unsigned number:{4u,5u,6u})require(actual.requestStream(1,number,3),"Actual event stream has independent native work");
      render(actual,480);actual.render(pcm.data(),512);
      require(std::any_of(pcm.begin(),pcm.end(),[](auto sample){return sample!=0;}),"Actual combined Disc1 streams produce decoded PCM");
      for(const auto &stream:originals)require(actual.streamRenderedFrames(stream->block,stream->number)==992,
          "All four actual PCM streams preserve their independent output clocks");
      // Align to the next native callback before applying the three actual
      // R100Main transitions. The oracle uses their real RIT default volumes.
      render(actual,208);
      std::vector<FadeOracle> sourceFades;
      for(unsigned i=1;i<originals.size();++i) {
        sourceFades.emplace_back(originals[i]->volume,0,400);
        actual.requestStream(1,originals[i]->number,4,400,0);
      }
      for(unsigned tick=0;tick<32768;++tick) {
        bool active=false;for(auto &fade:sourceFades) {fade.tick();active|=fade.playing;}
        render(actual,240);
        for(unsigned i=0;i<sourceFades.size();++i)require(
            actual.streamVolumeFixed(1,originals[i+1]->number)==unsigned(sourceFades[i].now) &&
            actual.streamActive(1,originals[i+1]->number)==sourceFades[i].playing,
            "Actual r100 event-stream fade follows original RIT volume and400 audio callbacks");
        if(!active)break;
      }
      require(actual.streamActive(0,code&255),"Fading all three actual block1 streams preserves actual room combat music");
    } else if(argc!=1)throw std::runtime_error("Usage: m37_streams_check [original-disc1-files-directory]");
    std::cout<<"M37 native stream work/fade: "<<checks<<" checks PASS, four concurrent PCM owners, exact5ms/signed8.8 oracle\n";
    return 0;
  } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
