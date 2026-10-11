#pragma once
#include "consumer_verification.h"
#include <cmath>
namespace re4::port::tests {
// Isolated format fixtures, explicitly not original room assets.
inline std::vector<u8> modelFixture() {
    std::vector<u8> b(0x90,0);
    word(b,0,0x50); word(b,0x30,0x70); word(b,0x3c,0x20010801);
    b[0x19]=2; b[0x28]=1; b[0x39]=2;
    b[0x51]=255; b[0x60]=1; b[0x61]=0;
    word(b,0x54,0x3f800000); word(b,0x64,0x40000000);
    const s16 v[8]={-4,-2,-3,0,8,4,5,0};
    for(std::size_t i=0;i<8;++i) { b[0x70+i*2]=u8(u16(v[i])>>8); b[0x71+i*2]=u8(v[i]); }
    return b;
}
inline std::vector<u8> textureFixture() {
    std::vector<u8> t(0x80,0);
    word(t,0,0x20af30); word(t,4,1); word(t,8,12); word(t,12,0x20);
    t[0x21]=8; t[0x23]=8; word(t,0x28,0x60);
    return t;
}
template<class Checks> void modelInitializationTests(Checks &c,GameRuntime &game) {
    game.initialize(); auto &engine=game.engineConsumers();
    auto bin=bytes(modelFixture()),tpl=bytes(textureFixture());
    c.rejects([&]{engine.initializeModel(bin,tpl,"fixture");},"model pool sizes must be explicit");
    engine.configureModelPools(2,8);
    auto a=engine.initializeModel(bin,tpl,"fixture"), b=engine.initializeModel(bin,tpl,"fixture-invisible",true);
    auto &m=engine.model(a); auto &info=*m.pModelInfo;
    auto *texture=static_cast<TexturePaletteView*>(info.tpl_addr);
    c.test(m.nParts==2 && m.pList && info.model_addr->resolved && texture->resolved,
           "actual modelInit and cModInfoMgr::create bind checked BIN/TPL views");
    c.test(m.getPartsPtr(0)->pParent==&m && m.getPartsPtr(1)->pParent==m.getPartsPtr(0),
           "actual original initJoint/setPartsParent");
    c.test(m.getPartsPtr(0)->world.x==1 && m.getPartsPtr(1)->world.x==3 &&
               m.getPartsPtr(1)->lt_inv_mat[0][3]==-3 && m.getPartsPtr(1)->world_old.x==3,
           "actual rest hierarchy, world and inverse bind matrices");
    c.test(info.bound.offset.x==1 && info.bound.offset.y==0.5f && info.bound.offset.z==0.5f &&
               info.bound.size.x==3 && info.bound.size.y==1.5f && info.bound.size.z==2,
           "original getBoundingBox over endian-converted signed vertices");
    m.pos.x=10; m.setPartsOffset(info.model_addr);
    c.test(m.getPartsPtr(1)->world.x==13 && m.getPartsPtr(1)->lt_inv_mat[0][3]==-3 &&
               m.Motion.Pos_world.x==10 && m.getPartsPtr(1)->world_old2.x==13,
           "original initialization preserves model translation and position-independent inverse bind matrices");
    c.test(texture->descriptors.size()==1 && texture->descriptors[0].data.size==32 &&
               info.colorWord==0xffffffffu && info.mat[0][0]==1,
           "original cModelInfo defaults and actual TPL descriptor binding");
    c.test(m.Motion.Seq_speed==1 && !m.Motion.pSeq_top && m.invisible_factor==1 &&
               engine.model(b).invisible_factor==0 && (m.be_flag&0x16)==0x16,
           "original motion defaults, flags and invisible setting; no motion playback claimed");
    c.rejects([&]{engine.initializeModel(bin,tpl,"full");},"original info-pool exhaustion must fail");
    c.test(engine.modelCount()==2 && engine.model(a).pModelInfo==&info,"failed initialization preserves existing models");
    engine.releaseModel(a); c.rejects([&]{engine.model(a);},"released model handle invalidates");
    for(int i=0;i<20;++i) {
        auto h=engine.initializeModel(bin,tpl,"reuse");
        c.test(engine.model(h).getPartsPtr(1)->world.x==3,"actual model pool reuse after release");
        engine.releaseModel(h);
    }
    engine.releaseModel(b);
    auto corrupt=modelFixture(); corrupt[0x61]=1;
    c.rejects([&]{engine.initializeModel(bytes(corrupt),tpl,"cycle");},"cycle rejected before original matrix traversal");
    corrupt=modelFixture(); word(corrupt,0x30,0x8f);
    c.rejects([&]{engine.initializeModel(bytes(corrupt),tpl,"range");},"vertex resource bounds");
    corrupt=modelFixture(); corrupt[0x28]=31;
    c.rejects([&]{engine.initializeModel(bytes(corrupt),tpl,"shift");},"original shift UB bounded");
    auto bad=textureFixture(); word(bad,0x28,0x7f);
    c.rejects([&]{engine.initializeModel(bin,bytes(bad),"texture-range");},"TPL image bounds");
    bad=textureFixture(); word(bad,16,4);
    c.rejects([&]{engine.initializeModel(bin,bytes(bad),"clut");},"unsupported CLUT cannot pretend success");
    engine.configureModelPools(8,4);
    a=engine.initializeModel(bin,tpl,"one"); b=engine.initializeModel(bin,tpl,"two");
    c.rejects([&]{engine.initializeModel(bin,tpl,"parts-full");},"original parts-pool exhaustion rollback");
    engine.releaseModel(a); auto h=engine.initializeModel(bin,tpl,"after-parts-failure");
    c.test(engine.model(h).nParts==2,"parts slots and info slot released after failed modelInit");
    engine.releaseModel(h); engine.releaseModel(b);
    c.test(engine.modelCount()==0,"all original model objects released");
    engine.configureModelPools(2,8);
    auto motionModel=engine.initializeModel(bin,tpl,"motion-target");
    std::vector<u8> fcv(128,0);
    fcv[1]=10; fcv[2]=1; fcv[4]=4; fcv[5]=1;
    word(fcv,8,128); word(fcv,12,16);
    for(u32 axis=0;axis<3;++axis) {
        auto at=16+axis*30; fcv[at+1]=2; fcv[at+5]=10;
        word(fcv,at+18,axis==0?0x41200000:axis==1?0x41a00000:0x41f00000);
    }
    auto motionBytes=bytes(fcv);
    engine.initializeMotion(motionModel,motionBytes,5,1,3);
    auto &work=engine.model(motionModel).Motion;
    c.test(work.Joint_num==1 && work.Seq_frame==5 && work.Hokan_cnt==3 && work.Seq_speed==1,
           "actual MotionSetCore preserves model speed and initializes hokan/sequence");
    auto sampled=engine.sampleMotion(motionModel,0,5);
    c.test(sampled.x==5 && sampled.y==10 && sampled.z==15,"actual original Hermite and BE FCC readers sample the midpoint");
    for(int repeat=0;repeat<12;++repeat) {
        engine.initializeMotion(motionModel,motionBytes,repeat%11);
        c.test(engine.model(motionModel).Motion.pMot && engine.model(motionModel).Motion.Seq_frame==repeat%11,
               "repeated original MotionClear/MotionSetCore retains owned buffers");
    }
    c.rejects([&]{engine.sampleMotion(motionModel,1,0);},"motion joint range");
    c.rejects([&]{engine.sampleMotion(motionModel,0,11);},"motion frame range");
    auto badMotion=fcv; badMotion[5]=2;
    c.rejects([&]{engine.initializeMotion(motionModel,bytes(badMotion));},"motion references missing model part");
    badMotion=fcv; word(badMotion,12,127);
    c.rejects([&]{engine.initializeMotion(motionModel,bytes(badMotion));},"unaligned/truncated FCV axis bounds");
    c.test(engine.model(motionModel).Motion.pMot && engine.sampleMotion(motionModel,0,10).x==10,
           "failed FCV validation preserves previous original motion work");
    auto keyed=std::vector<u8>(64,0);keyed[1]=10;keyed[2]=1;keyed[3]=0xf0;keyed[4]=4;keyed[5]=1;
    word(keyed,8,64);word(keyed,12,16);
    for(u32 axis=0;axis<3;++axis){auto at=16+axis*14;keyed[at+1]=2;keyed[at+5]=10;
        word(keyed,at+6,0x3f800000);word(keyed,at+10,axis==0?0x41200000:axis==1?0x41a00000:0x41f00000);}
    engine.initializeMotion(motionModel,bytes(keyed),0);
    c.test(engine.sampleMotion(motionModel,0,0).x==1,"original FCC15 reads an exact first key without tangents");
    sampled=engine.sampleMotion(motionModel,0,10);
    c.test(sampled.x==10&&sampled.y==20&&sampled.z==30,"original FCC15 holds the final key without tangents");
    c.rejects([&]{engine.sampleMotion(motionModel,0,5);},"FCC15 between-key values cannot invent uninitialized original tangents");
    EventResources resources;
    resources.registerBin("sample.fcv",motionBytes); resources.registerBin("sample.fcv",bin);
    c.test(resources.bin("sample.fcv").data==motionBytes.data && resources.BinTbl.pWork[0].Count==2,
           "actual DatTbl keeps first pointer and increments references");
    resources.BinTbl.DelDat("sample.fcv");
    c.test(resources.BinTbl.pWork[0].Count==1 && resources.bin("sample.fcv").data==motionBytes.data,
           "actual DatTbl release retains live reference");
    resources.BinTbl.DelDat("sample.fcv");
    c.test(!resources.bin("sample.fcv").data,"last original temporary table reference releases slot");
    resources.clear(); engine.releaseModel(motionModel);
    std::vector<u8> evd(0xcc,0);
    std::memcpy(evd.data(),"event",5); std::memcpy(evd.data()+0x20,"r120",5);
    std::memcpy(evd.data()+0x28,"s00",4);
    word(evd,0x40,0x50); word(evd,0x44,0x7c); word(evd,0x4c,0xcc);
    word(evd,0x50,3); evd[0x5d]=0x7c;
    std::memcpy(evd.data()+0x60,"fixture",8);
    std::memcpy(evd.data()+0x6c,"model.bin",10); std::memcpy(evd.data()+0x9c,"shared.tpl",11);
    auto package=indexEventPackage(bytes(evd)); auto refs=indexEventResourceReferences(package);
    c.test(refs.size()==1 && refs[0].bin=="model.bin" && refs[0].tpl=="shared.tpl" && refs[0].model=="fixture",
           "bounded original packet field positions preserve actual model/TPL references");
    evd[0x5d]=0x7d;
    c.rejects([&]{indexEventResourceReferences(indexEventPackage(bytes(evd)));},"packet cannot extend beyond original stream");
    evd[0x5d]=0;
    c.rejects([&]{indexEventResourceReferences(indexEventPackage(bytes(evd)));},"zero-length packet cannot loop forever");
    evd[0x5d]=0x7c; word(evd,0x50,33);
    c.rejects([&]{indexEventResourceReferences(indexEventPackage(bytes(evd)));},"original packet handler index bounded");
    game.initialize();
}
template<class Checks> void texRenderTests(Checks &c,GameRuntime &game) {
    game.initialize(); auto &tex=game.texRender();
    for(u32 i=0;i<8;++i) {
        auto *p=tex.initialize(i?0:256,int(i%3));
        c.test(p->IsAlive() && p->GetTexNo()==248+i && p->GetCoreFlg()==(8u<<i) &&
                   p->GetRepeatType()==int(i%3) && reinterpret_cast<std::uintptr_t>(p->GetBufAddr())%32==0,
               "original TexRender slot ID, mask, repeat and aligned owned memory");
    }
    c.test(tex.bufferBytes()==256*256*4+7*128*128*4,"original target allocation/reallocation sizes");
    c.rejects([&]{tex.initialize(128,0);},"original eight-target limit");
    c.test(tex.count()==8,"failed ninth target preserves slot count");
    c.rejects([&]{tex.copyToTexture();},"actual GX EFB copy stays a named missing dependency");
    tex.clear(); c.test(!tex.count() && !tex.bufferBytes(),"room target reset frees all owned buffers");
    c.rejects([&]{tex.initialize(65536,0);},"native target size overflow rejected");
    c.test(!tex.count() && !tex.bufferBytes(),"failed reallocation rolls back manager and original default buffer");
    c.rejects([&]{tex.initialize(128,3);},"invalid repeat option");
    for(int i=0;i<20;++i) {
        tex.initialize(256,2); tex.clear();
        c.test(!tex.count() && !tex.bufferBytes(),"repeat original target init and release");
    }
    game.initialize();
}
} // namespace re4::port::tests
