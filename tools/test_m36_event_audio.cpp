#include "sound_mixer.h"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
unsigned checks;
void require(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
std::shared_ptr<re4::SoundStream> fixture(unsigned block,int sample){auto stream=std::make_shared<re4::SoundStream>();stream->block=block;stream->number=block?2:0;stream->rate=re4::SoundMixer::OutputRate;stream->volume=127;stream->pan=64;stream->stereo=true;stream->looping=true;stream->left.assign(10000,std::int16_t(sample));stream->right=stream->left;stream->loopLeft=stream->left;stream->loopRight=stream->right;return stream;}
}
int main(int argc,char **argv){try{
 re4::SoundMixer room,event;auto bgm=fixture(0,500),speech=fixture(1,1000);room.installStream(bgm,true);event.installStream(speech,true);room.setPaused(false);event.setPaused(false);
 std::vector<std::int16_t>a(960),b(960);room.render(a.data(),480);event.render(b.data(),480);require(room.streamRenderedFrames()==480&&event.streamRenderedFrames()==480,"Independent stream clocks");require(a.back()!=0&&b.back()!=0&&a.back()+b.back()>a.back(),"Speech and BGM overlap");
 room.setPaused(true);event.setPaused(true);room.render(a.data(),480);event.render(b.data(),480);require(room.streamRenderedFrames()==480&&event.streamRenderedFrames()==480,"Background pause advanced stream clocks");require(std::all_of(a.begin(),a.end(),[](auto x){return !x;})&&std::all_of(b.begin(),b.end(),[](auto x){return !x;}),"Paused audio is not silent");
 room.setPaused(false);event.setPaused(false);room.render(a.data(),480);event.render(b.data(),480);require(room.streamRenderedFrames()==960&&event.streamRenderedFrames()==960,"Foreground resumed wrong stream position");
 event.installStream(nullptr);room.installStream(bgm);require(room.streamRenderedFrames()==960&&event.streamRenderedFrames()==0,"Completion restarted BGM or retained event speech");room.render(a.data(),480);event.render(b.data(),480);require(room.streamRenderedFrames()==1440&&event.streamRenderedFrames()==0,"Post-event room stream stopped");require(a.back()!=0&&std::all_of(b.begin(),b.end(),[](auto x){return !x;}),"Post-event music/speech ownership");
 room.clear();event.clear();require(room.streamRenderedFrames()==0&&event.streamRenderedFrames()==0,"Room reset did not clear stream counters");
 auto shortStream=fixture(1,1000);shortStream->looping=false;shortStream->left.resize(7);shortStream->right.resize(7);event.installStream(shortStream,true);event.setPaused(false);event.render(b.data(),480);require(event.streamRenderedFrames()==7,"Finished voice clock did not retain actual consumed frames");event.render(b.data(),480);require(event.streamRenderedFrames()==7&&std::all_of(b.begin(),b.end(),[](auto x){return !x;}),"Finished voice continued/rewound");
 event.installStream(speech,true);std::atomic<bool>failed{false};std::thread consumer([&]{try{for(unsigned i=0;i<500;++i)event.render(b.data(),480);}catch(...){failed=true;}});for(unsigned i=0;i<500;++i){event.setPaused((i%7)==0);(void)event.streamRenderedFrames();if(i%25==0)event.installStream(speech,true);}consumer.join();require(!failed,"Concurrent event stream lifecycle failed");
 if(argc==2){std::string root=argv[1];const auto voice=re4::SoundStream::Load(root+"/bgm/bio4str.hed",root+"/bgm/bio4evt.sbb",1,2),combat=re4::SoundStream::Load(root+"/bgm/bio4str.hed",root+"/bgm/bio4bgm.sbb",0,0);room.installStream(combat,true);event.installStream(voice,true);room.setPaused(false);event.setPaused(false);room.render(a.data(),480);event.render(b.data(),480);require(room.streamRenderedFrames()==480&&event.streamRenderedFrames()==480,"Supplied source BGM/speech clocks");event.installStream(nullptr);room.installStream(combat);require(room.streamRenderedFrames()==480,"Supplied combat BGM restarted on event finish");}
 std::cout<<"M36 independent event speech/room BGM/pause/lifecycle PASS: "<<checks<<" checks\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
