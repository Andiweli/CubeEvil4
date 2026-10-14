#include "radio_term.h"
#include "asset_reader.h"
#include "weapon_preview.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <set>
#include <stdexcept>
namespace re4 {
namespace {
using Bytes=std::vector<std::uint8_t>;
Bytes read(const std::string &path) {
 std::ifstream f(path,std::ios::binary|std::ios::ate);const auto n=f.tellg();
 if(!f || n<16 || n>32*1024*1024)throw std::runtime_error("Radio: Original file missing/budget: "+path);
 Bytes b(std::size_t(n),0);f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("Radio: Truncated original file");return b;
}
float curve(const Bytes &b,unsigned offset,float time,float fallback) {
 if(!offset)return fallback;
 const AssetReader r{b};const auto n=r.u32(offset);
 if(!n || n==0xffffffff)return fallback;
 if(n>64)throw std::runtime_error("Radio: ID curve budget");
 r.range(offset+4,n*16);
 // Original Hermite_1CurveCalc returns0 for a one-point/no-segment curve.
 for(unsigned i=0;i+1<n;++i){const auto a=offset+4+i*16,c=a+16;
  const float ta=r.f32(a),tb=r.f32(c);if(time<ta || time>tb)continue;
  if(tb<=ta)throw std::runtime_error("Radio: ID key order");
  const float dt=tb-ta,s=(time-ta)/dt,s2=s*s,s3=s*s2,h01=-(s3+s3)+3*s2,h11=s3-s2,h10=h11-s2+s,h00=1-h01;
  return h00*r.f32(a+4)+h01*r.f32(c+4)+dt*(h10*r.f32(a+8)+h11*r.f32(c+12));
 }return 0;
}
struct Unit {
 SubscreenQuad q;Bytes bytes;unsigned p{},rev{},loop{},sizeFlag{},axis{},textureFlag{},image{},maskImage{};
 std::array<unsigned,4> timer{};std::array<unsigned,6> offsets{};
 Matrix34 matrix{1,0,0,0,0,1,0,0,0,0,1,0};bool visible=true,display=true;
};
void advanceTimer(Unit &u,unsigned k) {
 const AssetReader r{u.bytes};const auto c=u.offsets[k+2];if(!c)return;const auto n=r.u32(c);if(!n || n==0xffffffff)return;
 const int end=int(r.f32(c+4+(n-1)*16));int time=int(std::int16_t(u.timer[k]));
 if(u.rev&(1<<k)){--time;if(time<=0)time=u.loop&(1<<k)?end:0;}
 else{++time;if(time>=end)time=u.loop&(1<<k)?0:end;}u.timer[k]=unsigned(std::uint16_t(time));
}
std::vector<Unit> units(const Bytes &b,unsigned group) {
 auto quads=DecodeSubscreenLayout(b,group);std::vector<Unit> out;
 if(b[0]!='2')throw std::runtime_error("Radio: expected original ID2 layout is missing");
 for(unsigned i=0;i<quads.size();++i){Unit u;u.q=quads[i];u.bytes=b;u.p=8+i*140;const AssetReader r{b};
  u.rev=b[u.p+16];u.loop=b[u.p+13];u.sizeFlag=b[u.p+14];u.axis=b[u.p+15];u.textureFlag=b[u.p+111];
  for(unsigned k=0;k<6;++k)u.offsets[k]=r.u32(u.p+116+k*4);
  if(u.offsets[0] && b[u.offsets[0]+7]!=2)throw std::runtime_error("Radio: unverified ID path");
  if(group==9 && (u.q.mark==0x12 || u.q.mark==0x13 || u.q.mark==0x14)){u.visible=false;u.rev|=15;}
  out.push_back(std::move(u));
 }return out;
}
}
struct RadioTermData::Impl {
 unsigned call{},language{};SubscreenFont font;std::vector<RadioTermSequence> sequence;
 std::vector<Bytes> models,tpls;Bytes idleLeon,idlePartner,talkLeon,talkPartner,shapeLeon,shapePartner,layoutBack,layoutFront;
 std::vector<SubscreenTexture> textures;std::map<unsigned,unsigned> patterns;std::vector<std::vector<std::uint16_t>> messages;
 std::shared_ptr<const SoundStream> voice;std::shared_ptr<const RoomLighting> light;
};
RadioTermData::RadioTermData(const std::string &files,unsigned call,unsigned language) {
 if(call>12 || language>6)throw std::runtime_error("Radio: r100 call/language");
 auto d=std::make_shared<Impl>();d->call=call;d->language=language;
 // sscrnSetLanguage has both US/UK English entries. MessageControl::setLanguage
 // collapses those two entries onto MDT language1; the other MDT slots follow.
 static const char *langs[]{"Jpn","Eng","Eng","Ger","Fra","Esp","Ita"};
 const unsigned messageLanguage=language>1?language-1:language;
 const auto term=read(files+"/ss/"+langs[language]+"/ss_term.dat"),common=read(files+"/ss/"+langs[language]+"/ss_cmmn.dat");
 static const char *partners[]={"101","102","103a","103b","104","105","106","107","108","109","110","111","112"};
 const auto partner=read(files+"/ss/Cmn/ss_oc"+partners[call]+".dat");auto op=read(files+"/op/op01.das");op.erase(op.begin(),op.begin()+0x400);
 const auto player=RoomArchive::LoadDrs(files+"/em/pl00.drs");const auto pl=[&](unsigned arc){return player.copy(arc-4);};
 d->models={pl(4),pl(10),CopySubscreenEntry(term,10),CopySubscreenEntry(term,12),pl(9)};
 d->tpls={pl(5),pl(5),CopySubscreenEntry(term,11),CopySubscreenEntry(term,13),pl(7)};
 for(unsigned i=6;i<=11;++i){d->models.push_back(CopySubscreenEntry(partner,i));d->tpls.push_back(CopySubscreenEntry(partner,5));}
 d->idleLeon=CopySubscreenEntry(term,14);d->idlePartner=CopySubscreenEntry(partner,4);
 d->talkLeon=CopySubscreenEntry(partner,12);d->talkPartner=CopySubscreenEntry(partner,14);
 d->shapeLeon=CopySubscreenEntry(partner,13);d->shapePartner=CopySubscreenEntry(partner,15);
 d->layoutBack=CopySubscreenEntry(term,8);d->layoutFront=CopySubscreenEntry(term,9);
 d->font=language?LoadOriginalMenuFont(files):DecodeSubscreenFont(CopySubscreenEntry(term,4),28,28);
 const auto sequence=CopySubscreenEntry(op,17+call);const AssetReader sr{sequence};
 for(unsigned p=0;p+16<=sequence.size();p+=16){RadioTermSequence s{sr.u16(p),std::int16_t(sr.u16(p+2)),std::int32_t(sr.u32(p+4)),std::int32_t(sr.u32(p+8)),std::int32_t(sr.u32(p+12))};
  if(s.frame<0 || (!d->sequence.empty() && s.frame<d->sequence.back().frame))throw std::runtime_error("Radio: Original OpeSeq time");
  d->sequence.push_back(s);if(s.timer==-1)break;
 }if(d->sequence.empty() || d->sequence.back().timer!=-1)throw std::runtime_error("Radio: OpeSeq end is missing");
 const auto mdt=CopySubscreenEntry(op,4+call);const AssetReader mr{mdt};const auto block=mr.u32((messageLanguage+1)*4),count=mr.u32(block+4);
 if(count>64)throw std::runtime_error("Radio: Original MDT budget");
 for(unsigned i=0;i<count;++i)d->messages.push_back(ReadSubscreenMessage(mdt,i,messageLanguage));
 // Id textures register CORE, shared screen and terminal in that order;
 // cTexSys::TexRegist keeps a previously registered id;0x80 only suppresses
 // its duplicate diagnostic, it does not replace the existing texture.
 const auto coreFile=read(files+"/etc/core.das");const AssetReader cr{coreFile};const auto co=cr.u32(0x2c),cl=cr.u32(0x24);cr.range(co,cl);
 const Bytes core(coreFile.begin()+co,coreFile.begin()+co+cl);
 for(const auto &pack:std::vector<Bytes>{CopySubscreenEntry(core,0x1d),CopySubscreenEntry(common,6),CopySubscreenEntry(term,5)}){
  const AssetReader r{pack};const auto ids=r.u32(4),anms=r.u32(28),n=r.u32(ids);
  auto tex=DecodeSubscreenIdTextures(pack);std::set<unsigned> accepted;
  for(unsigned i=0;i<n;++i){const unsigned id=r.u16(ids+4+i*8)&255,a=anms+r.u32(anms+4+i*4);
   if(!d->patterns.count(id)){d->patterns[id]=r.u16(a+8);accepted.insert(id);}
  }for(auto &t:tex)if(accepted.count(t.id))d->textures.push_back(std::move(t));
 }
 auto lights=std::make_shared<RoomLighting>(CopySubscreenEntry(common,0x15));const RoomLighting coreLight(CopySubscreenEntry(core,0x0b));
 const auto &environment=coreLight.cuts.at(9);auto &cut=lights->cuts.at(0);
 // LightMgr.create(CORE,9,-2) installs the complete environment; the
 // terminal supplies three independent lights via sscrnLightCreate.
 cut.lights.resize(3);auto terminalLights=std::move(cut.lights);
 cut=environment;cut.lights=std::move(terminalLights);
 for(auto &l:cut.lights)l.flags|=3;
 d->light=lights;d->voice=SoundStream::Load(files+"/bgm/bio4str.hed",files+"/bgm/bio4evt.sbb",1,0x8c+call);impl_=std::move(d);
}
RadioTermData::~RadioTermData()=default;
unsigned RadioTermData::callNo()const{return impl_->call;}unsigned RadioTermData::language()const{return impl_->language;}
const SubscreenFont &RadioTermData::font()const{return impl_->font;}
std::shared_ptr<const SoundStream> RadioTermData::voice()const{return impl_->voice;}
const std::vector<RadioTermSequence> &RadioTermData::sequence()const{return impl_->sequence;}
struct RadioTermRuntime::Impl {
 std::shared_ptr<const RadioTermData> data;std::vector<std::unique_ptr<GenericEventActor>> actors;
 std::vector<Unit> units;std::vector<SubscreenQuad> quads;std::vector<RadioTermAudio> audio;
 R100RoomMessage message;unsigned frame{},seqFrame{},seqIdx{},wait=30,mesWait{},shapeFrame{},line{},color{},cursor{},messageWait{};
 bool finished{},manual{},cancelled{},requested{},started{},shapeActive{},messageHidden=true,endPending{};
 void motions(bool talking,unsigned blend){for(unsigned i:{0u,5u})actors[i]->rig()->startActionFlags(talking?1:0,blend,0,talking?0x8000:0x8004);
  if(talking){actors[3]->startShape(std::make_shared<EventCurve>(data->impl_->shapeLeon),0);actors[8]->startShape(std::make_shared<EventCurve>(data->impl_->shapePartner),0,1.37f);shapeFrame=0;shapeActive=true;}
  else{actors[3]->clearShape();actors[8]->clearShape();shapeActive=false;}
 }
 void showFrame(){for(auto &u:units)if(u.q.group==9 && (u.q.mark==0x13 || u.q.mark==0x14)){u.visible=true;u.rev&=~15u;u.timer.fill(0);
   // IDSystem::setTime recursively resets every child timer.
   for(auto &c:units)if(c.q.group==9 && c.q.parent==u.q.unit)c.timer.fill(0);
 }}
 void stop(){if(requested){audio.push_back({RadioTermAudio::StreamStop,1,0x8c+data->callNo()});requested=false;}}
 void finish(){stop();finished=true;message.glyphs.clear();audio.push_back({RadioTermAudio::Sample,0,0x15});}
 void setMessage(int no,int timer){messageHidden=false;mesWait=unsigned(std::max(timer,0));
  if(no==-1){messageWait=1;return;}
  if(unsigned(no)>=data->impl_->messages.size())throw std::runtime_error("Radio: Original MDT number");
  message={};message.number=no;message.attributes=0x03000054;message.x=256;message.y=260;message.font=data->language()?0:3;
  message.fontWidth=data->language()?21:24;message.fontHeight=data->language()?28:21;message.lineGap=25;message.words=data->impl_->messages[no];
  const auto *position=static_cast<const Unit*>(nullptr);for(const auto &u:units)if(u.q.group==9 && u.q.mark==0xfe)position=&u;
  if(position){message.x=unsigned(int((position->q.position[0]+320)*.8f));message.y=unsigned(int((240-position->q.position[1])*.8f));}
  cursor=line=color=messageWait=0;endPending=false;
 }
 bool seq(){if(seqIdx>=data->sequence().size())throw std::runtime_error("Radio: OpeSeq budget");const auto &s=data->sequence()[seqIdx];
  if(s.timer==-1){finish();return false;}setMessage(s.message,s.timer);++seqIdx;return true;
 }
 void text(){if(messageHidden || message.words.empty())return;
  for(unsigned budget=0;budget<4096;++budget){if(cursor>=message.words.size())throw std::runtime_error("Radio: MDT end is missing");const auto c=message.words[cursor];
   if(c>=128){if(message.glyphs.size()>=256)throw std::runtime_error("Radio: Glyph budget");message.glyphs.push_back({unsigned(c-128),line,color});++cursor;continue;}
   switch(c){case 0:message.glyphs.clear();line=color=0;message.pageStart=++cursor;break;
    case 1:endPending=true;return;
    case 3:++line;++cursor;break;
    case 4:message.glyphs.clear();line=color=0;message.pageStart=++cursor;return;
    case 6:color=message.words.at(cursor+1);cursor+=2;break;
    case 8:++cursor;break; // attr02000000 suppresses button wait.
    case 9:if(!messageWait){messageWait=message.words.at(cursor+1);return;}if(messageWait==65535)return;if(--messageWait)return;cursor+=2;break;
    case 18:cursor+=2;break; // Speaker id, not a font or visual substitute.
    default:throw std::runtime_error("Radio: unsupported original MDT opcode "+std::to_string(c));
   }
  }throw std::runtime_error("Radio: MDT control budget");
 }
 void ids(){quads.clear();const auto &patterns=data->impl_->patterns;
  for(unsigned level=0;level<8;++level)for(auto &u:units){const AssetReader r{u.bytes};const auto p=u.p;if(u.bytes[p+6]!=level)continue;
   auto &q=u.q;for(unsigned k=0;k<3;++k)q.position[k]=r.f32(p+20+k*4);
   if(u.offsets[0]){const auto path=u.offsets[0];const float t=curve(u.bytes,u.offsets[2],float(std::int16_t(u.timer[0])),0);
    for(unsigned k=0;k<3;++k){const float p0=r.f32(path+8+k*4),p1=r.f32(path+20+k*4);float point=0;point+=(1-t)*p0;point+=t*p1;point-=p0;q.position[k]+=point;}
    advanceTimer(u,0);}
   const float scale=curve(u.bytes,u.offsets[3],float(std::int16_t(u.timer[1])),1),w=r.f32(p+80)*(u.sizeFlag&32?1:scale),h=r.f32(p+84)*(u.sizeFlag&16?1:scale);
   float x=-w*.5f,y=h*.5f;if(q.anchor==1){x=0;y=0;}else if(q.anchor==2){x=-w;y=0;}else if(q.anchor==3){x=-w;y=h;}else if(q.anchor==4){x=0;y=h;}
   q.vertices={x,y,x+w,y,x+w,y-h,x,y-h};advanceTimer(u,1);
   const float c=curve(u.bytes,u.offsets[4],float(std::int16_t(u.timer[2])),u.bytes[p+91]);
   const bool animateColor=u.offsets[4] && r.u32(u.offsets[4])!=0;
   for(unsigned k=0;k<4;++k)q.color[k]=std::clamp(!animateColor?float(u.bytes[p+88+k]):r.u32(p+92)?(1-c)*u.bytes[p+88+k]+c*u.bytes[p+92+k]:k==3?c:float(u.bytes[p+88+k]),0.f,255.f);
   advanceTimer(u,2);
   for(unsigned k=0;k<3;++k)q.rotation[k]=r.f32(p+96+k*4);
   if(u.offsets[5] && u.axis<3)q.rotation[u.axis]=curve(u.bytes,u.offsets[5],float(std::int16_t(u.timer[3])),0);
   advanceTimer(u,3);
   SmdPlacement placement;placement.position=q.position;placement.rotation=q.rotation;for(float &v:placement.rotation)v*=.017453292f;placement.scale={1,1,1};u.matrix=PlacementMatrix(placement);
   const Unit *parent=nullptr;for(const auto &other:units)if(other.q.group==q.group && other.q.unit==q.parent){parent=&other;break;}
   bool display=u.visible;if(parent){u.matrix=WeaponAttachmentMatrix(u.matrix,parent->matrix);display=display&&parent->display;for(unsigned k=0;k<4;++k)q.color[k]=float(std::uint8_t(q.color[k]*parent->q.color[k]/255));}
   if(!(u.textureFlag&2) && patterns.count(q.texture))u.image=(frame?frame-1:0)%patterns.at(q.texture);
   if(!(u.textureFlag&4) && patterns.count(q.mask))u.maskImage=(frame?frame-1:0)%patterns.at(q.mask);
   u.display=display;
   if(q.kind || q.texture==255 || !display)continue;
   for(unsigned k=0;k<4;++k){const auto v=TransformPoint(u.matrix,{q.vertices[k*2],q.vertices[k*2+1],0});q.vertices[k*2]=v[0];q.vertices[k*2+1]=v[1];}
   q.position={0,0,0};q.rotation={0,0,0};quads.push_back(q);
  }
  auto rendered=std::move(quads);quads.clear();
  // unitTrans queues children before their owner, then AddOtDirect inserts
  // at the bucket head. ExecOt therefore draws the reversed postorder.
  std::function<void(const Unit&,unsigned)> queue=[&](const Unit &u,unsigned depth){
   if(depth>8)throw std::runtime_error("Radio: Original ID parent cycle");
   if(!u.display)return;
   if(u.q.kind==1)for(const auto &child:units)if(child.q.group==u.q.group && child.q.parent==u.q.unit)queue(child,depth+1);
   for(const auto &q:rendered)if(q.group==u.q.group && q.unit==u.q.unit){quads.push_back(q);break;}
  };
  for(const auto &u:units)if(u.q.parent==255)queue(u,0);
  std::reverse(quads.begin(),quads.end());
 }
};
RadioTermRuntime::RadioTermRuntime(std::shared_ptr<const RadioTermData> data):impl_(std::make_unique<Impl>()) {
 if(!data)throw std::runtime_error("Radio: Data is missing");
 auto &w=*impl_;w.data=std::move(data);const auto &d=*w.data->impl_;
 for(unsigned i=0;i<d.models.size();++i){GenericEventActor *parent=i==0 || i==5?nullptr:w.actors[i<5?0:5].get();auto a=std::make_unique<GenericEventActor>("radio_"+std::to_string(i),"original_ss_term",d.models[i],d.tpls[i],parent);a->setDisplayOverride(true);a->setLightMaskOverride(1);w.actors.push_back(std::move(a));}
 w.actors[0]->prepareMotions({{"0idle",d.idleLeon},{"1talk",d.talkLeon}});w.actors[5]->prepareMotions({{"0idle",d.idlePartner},{"1talk",d.talkPartner}});
 SmdPlacement pl;pl.position={435,-1580,850};pl.rotation={0,std::atan2(-435.f,1150.f),0};pl.scale={1,1,1};w.actors[0]->setPlacement(pl);pl.position={0,-1600,950};pl.rotation={0,0,0};w.actors[5]->setPlacement(pl);
 w.units=units(d.layoutBack,8);auto front=units(d.layoutFront,9);w.units.insert(w.units.end(),std::make_move_iterator(front.begin()),std::make_move_iterator(front.end()));
 w.motions(false,0);w.audio.push_back({RadioTermAudio::Sample,0,0x14});w.ids();
}
RadioTermRuntime::~RadioTermRuntime()=default;
void RadioTermRuntime::tick30Hz(bool confirm,bool skip,bool close,bool advance,bool ready){auto &w=*impl_;if(!advance || w.finished)return;++w.frame;
 if(w.manual && !w.cancelled){w.motions(false,10);w.showFrame();w.cancelled=true;}
 for(unsigned i:{0u,5u})w.actors[i]->rig()->advanceAction();
 if(w.shapeActive){w.actors[3]->sampleShape(w.shapeFrame);w.actors[8]->sampleShape(w.shapeFrame);++w.shapeFrame;}
 if(w.wait)--w.wait;
 else{
  if(!w.requested && !w.started){w.audio.push_back({RadioTermAudio::StreamRequest,1,0x8c+w.data->callNo()});w.requested=true;if(close)w.finish();w.ids();return;}
  if(!w.started){if(!ready){if(close)w.finish();w.ids();return;}w.audio.push_back({RadioTermAudio::StreamStart,1,0x8c+w.data->callNo()});w.started=true;w.motions(true,10);w.showFrame();}
  if(w.mesWait && !--w.mesWait)w.messageHidden=true;
  if(!w.manual){if(skip){w.stop();w.manual=true;}while(!w.finished && w.data->sequence()[w.seqIdx].frame<=int(w.seqFrame))if(!w.seq())break;
   // OpeMesMove returns immediately at the Timer==-1 sentinel, before seqCnt++.
   if(!w.finished)++w.seqFrame;}
  else if(confirm || skip)w.seq();
  if(close && !w.finished)w.finish();
 }w.text();w.ids();}
bool RadioTermRuntime::finished()const{return impl_->finished;}bool RadioTermRuntime::manual()const{return impl_->manual;}
unsigned RadioTermRuntime::frame()const{return impl_->frame;}unsigned RadioTermRuntime::sequenceFrame()const{return impl_->seqFrame;}
unsigned RadioTermRuntime::fadeAlpha()const{return impl_->frame<5?255-impl_->frame*51:0;}
const RadioTermData &RadioTermRuntime::data()const{return *impl_->data;}
const R100RoomMessage &RadioTermRuntime::message()const{static const R100RoomMessage empty;return impl_->messageHidden?empty:impl_->message;}
const std::vector<std::unique_ptr<GenericEventActor>> &RadioTermRuntime::actors()const{return impl_->actors;}
const std::vector<SubscreenQuad> &RadioTermRuntime::quads()const{return impl_->quads;}
const std::vector<SubscreenTexture> &RadioTermRuntime::textures()const{return impl_->data->impl_->textures;}
std::shared_ptr<const RoomLighting> RadioTermRuntime::lighting()const{return impl_->data->impl_->light;}
std::vector<RadioTermAudio> RadioTermRuntime::takeAudio(){auto out=std::move(impl_->audio);impl_->audio.clear();return out;}
unsigned RadioTermRuntime::textureFrame(unsigned group,unsigned unit,bool mask)const{for(const auto &u:impl_->units)if(u.q.group==group && u.q.unit==unit)return mask?u.maskImage:u.image;throw std::runtime_error("Radio: Original ID unit is missing");}
std::array<unsigned,2> RadioTermRuntime::textureEffect(unsigned group,unsigned unit)const{for(const auto &u:impl_->units)if(u.q.group==group && u.q.unit==unit)return {u.bytes[u.p+109],u.bytes[u.p+112]};throw std::runtime_error("Radio: Original ID effect is missing");}
}
