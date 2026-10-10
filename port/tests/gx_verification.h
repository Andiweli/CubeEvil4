#pragma once
#include "../src/gx_original.h"
#include <iostream>
namespace re4::port {
// Synthetic wire-format adapter checks; these are never presented as a game scene.
inline int verifyGxAdapters() {
    u32 checks=0;
    auto check=[&](bool c){++checks;if(!c)throw std::runtime_error("GX adapter assertion "+std::to_string(checks));};
    auto fail=[&](auto f){bool rejected=false;try{f();}catch(const std::exception &){rejected=true;}check(rejected);};
    auto bytes=std::make_shared<std::vector<u8>>(256,0);
    auto put16=[&](u32 p,u16 w){bytes->at(p)=u8(w>>8);bytes->at(p+1)=u8(w);};
    auto put32=[&](u32 p,u32 w){put16(p,u16(w>>16));put16(p+2,u16(w));};
    put32(0,64);put32(12,160);put32(16,176);put32(20,192);put32(28,192);put32(32,0xa0000000);put32(48,96);put32(52,128);put16(56,4);put16(58,4);put16(26,1);put32(60,0x20010801);(*bytes)[25]=1;put32(216,32);
    for(u32 i=0;i<4;++i){put16(96+i*8,u16(i));(*bytes)[128+i*4+2]=64;(*bytes)[160+i*4+3]=255;put16(176+i*4,u16(i*256));}
    auto lease=[&](){return ResourceLease{bytes,bytes->data(),u32(bytes->size())};};
    Mtx world;C_MTXIdentity(world);
    auto primitive=[&](u8 op,u16 count){std::fill(bytes->begin()+224,bytes->end(),u8(0));(*bytes)[224]=op;put16(225,count);for(u32 i=0;i<count;++i){put16(227+i*8,u16(i));put16(229+i*8,u16(i));put16(231+i*8,u16(i));put16(233+i*8,u16(i));}};
    // Four indices require 35 bytes -> aligned list size 64, not 32.
    bytes->resize(288);put32(216,64);primitive(0x80,4);GxScene quad;decodeGxModel(lease(),world,0,1,quad,{});
    check(quad.indices==std::vector<u32>{0,1,2,0,2,3});check(quad.vertices.size()==4);check(quad.vertices[3].position[0]==3);check(quad.vertices[3].uv[0]==3);check(quad.vertices[0].normal[2]==1);
    primitive(0x98,4);GxScene strip;decodeGxModel(lease(),world,0,1,strip,{});check(strip.indices==std::vector<u32>{0,1,2,2,1,3});
    primitive(0x90,3);GxScene tri;decodeGxModel(lease(),world,0,1,tri,{});check(tri.indices==std::vector<u32>{0,1,2});
    (*bytes)[224]=0x61;fail([&]{GxScene s;decodeGxModel(lease(),world,0,1,s,{});});
    primitive(0x90,3);put16(227,4);fail([&]{GxScene s;decodeGxModel(lease(),world,0,1,s,{});});
    primitive(0x90,3);put16(233,65535);fail([&]{GxScene s;decodeGxModel(lease(),world,0,1,s,{});});
    primitive(0x90,3);(*bytes)[204]=1;fail([&]{GxScene s;decodeGxModel(lease(),world,0,1,s,{});});(*bytes)[204]=0;
    for(u32 size=0;size<288;++size)fail([&]{GxScene s;auto b=lease();b.size=size;decodeGxModel(b,world,0,1,s,{});});
    // Formats + known TLUT values, tiled writes, alpha and channel planes.
    for(u32 fmt:{0u,1u,2u,3u,4u,5u,6u,8u,9u,10u,14u}){
        bytes=std::make_shared<std::vector<u8>>(256,0);put32(0,0x20af30);put32(4,1);put32(8,12);put32(12,32);put16(32,1);put16(34,1);put32(36,fmt);put32(40,128);
        if(fmt==0)(*bytes)[128]=0xf0;
        if(fmt==1||fmt==2)(*bytes)[128]=255;
        if(fmt==3){(*bytes)[128]=255;(*bytes)[129]=255;}
        if(fmt==4)put16(128,0xffff);
        if(fmt==5)put16(128,0xffff);
        if(fmt==6){(*bytes)[128]=255;(*bytes)[129]=255;(*bytes)[160]=255;(*bytes)[161]=255;}
        if(fmt==14){put16(128,0xffff);put16(130,0);}
        if(fmt>=8&&fmt<=10){put32(16,80);put16(80,1);put32(84,1);put32(88,192);put16(192,0xffff);}
        auto decoded=decodeGxTpl(lease());check(decoded.size()==1);check(decoded[0].levels[0].rgba==std::vector<u8>{255,255,255,255});
        fail([&]{auto b=lease();b.size=128;decodeGxTpl(b);});
    }
    put32(36,13);fail([&]{decodeGxTpl(lease());});
    put32(36,9);(*bytes)[128]=1;fail([&]{decodeGxTpl(lease());});
    float projection[4][4];C_MTXPerspective(projection,50,1.3333334f,100,1000000);
    auto z=[&](float p){float c=projection[2][2]*p+projection[2][3],w=-p;return (c+w)/w;};check(std::fabs(z(-100))<1e-6f);check(std::fabs(z(-1000000)-1)<1e-6f);
    GxCamera c;c.param.Campos={0,0,10};c.param.Target={0,0,0};CameraSetOrientationRoll(&c);C_MTXLookAt(c.view,&c.param.Campos,&c.Up,&c.param.Target);Vec origin{},view;C_MTXMultVec(c.view,&origin,&view);check(view.x==0&&view.y==0&&view.z==-10);
    std::cout<<"{\"kind\":\"isolated_GX_adapter_tests_with_original_SDK_matrices\",\"pointer_bits\":"<<sizeof(void*)*8<<",\"checks\":"<<checks<<",\"failed\":0}\n";return 0;
}
}
