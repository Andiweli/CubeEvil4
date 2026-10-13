#pragma once
#include "model_verification.h"
#include "../src/model_original.h"
namespace re4::port::tests {
// Isolated BE record/manager tests which execute original calcParent/getCoord/
// calcPos/getNormal/loadLit. The synthetic owners are never called Leon actors.
template<class Checks> void lightParentTests(Checks &c,GameRuntime &game) {
    game.initialize();auto &e=game.engineConsumers();e.configureModelPools(1,4);
    auto h=e.initializeModel(bytes(modelFixture()),bytes(textureFixture()),"light-parent adapter");
    auto &m=e.model(h);auto *part=m.getPartsPtr(1);
    C_MTXRotRad(part->mat,'z',1.57079632679f);part->mat[0][3]=10;part->mat[1][3]=20;part->mat[2][3]=30;
    auto blob=std::vector<u8>(8+0x104+0x12c,0);blob[1]=1;blob[2]=0x2c;blob[3]=1;word(blob,4,8);word(blob,12,1);
    const auto at=8+0x104;blob[at]=3;blob[at+1]=3;blob[at+3]=0x10;
    word(blob,at+4,0x3f800000);word(blob,at+8,0x40000000);word(blob,at+12,0x40400000);
    blob[at+20]=120;blob[at+21]=100;blob[at+22]=80;blob[at+23]=128;word(blob,at+24,0x3f800000);word(blob,at+32,0x10007);
    GameplayHeaps heaps;NativeGameState state;LightRuntime lights(heaps,game.loader().scheduler(),e,state);
    for(u8 type:{u8(1),u8(2),u8(3),u8(4)}){
        blob[at+28]=type;auto b=bytes(blob);lights.initialize();lights.bindRoom(b,b,b,2);
        lights.setModelResolver([&](u8 t,u16 no)->std::optional<ConsumerHandle>{c.test(t==type&&no==7,"original parent ID lookup keeps low/high halves");return h;});
        auto eventBlock=lights.bindEventBlock(b);c.test(eventBlock==3&&lights.block(3)->header()->Version==0x2c,"event LIT is owned and upgraded by original code");
        lights.selectBlock(-1);c.test(lights.roomSelected(),"event resource replacement restores original room LIT");lights.selectBlock(eventBlock);
        lights.loadRenderCut(eventBlock,0);auto pool=lights.renderPool();auto p=pool[0].fields.World;
        c.test(std::fabs(p.x-8)<1e-5f&&std::fabs(p.y-21)<1e-5f&&p.z==33,"original parent part local-to-world position");
        auto n=lights.transformDirection(0,{1,0,0});c.test(std::fabs(n.x)<1e-5f&&std::fabs(n.y-1)<1e-5f&&n.z==0,"original direction excludes translation");
        c.test(pool[0].fields.DispCol.r==120&&pool[0].fields.DispCol.a==128,"original create already invokes Light00_Move");
        c.rejects([&]{e.releaseModel(h);},"live light prevents model pool retirement");
        c.rejects([&]{e.clear();},"live light prevents manager clear");
        part->mat[0][3]=11;lights.refreshParentCoordinates();c.test(std::fabs(lights.renderPool()[0].fields.World.x-9)<1e-5f,"parent movement refreshes original coordinate");part->mat[0][3]=10;
        lights.clear();c.test(e.model(h).nParts==2,"clearing lights leaves their model alive");
    }
    auto pin=e.borrowModel(h);c.rejects([&]{e.configureModelPools(2,8);},"live model prohibits pool relocation");pin.reset();e.releaseModel(h);
    c.rejects([&]{e.model(h);},"retired parent handle stays invalid");
}
}
