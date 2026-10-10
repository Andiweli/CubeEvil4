#include "re4/port/gx_scene.h"
#include "re4/port/game_runtime.h"
#include <iostream>
#include "gx_verification.h"
int main(int argc,char **argv) {
    try {
        if(argc==2 && std::string(argv[1])=="self-test")return re4::port::verifyGxAdapters();
        if(argc!=8 || std::string(argv[1])!="scene")throw std::invalid_argument("scene <root> <catalog> <disc> <room-hex> <event-FST-path> <frame>");
        auto disc=std::stoul(argv[4]);auto room=std::stoul(argv[5],nullptr,16);auto frame=std::stoul(argv[7]);
        if(disc>1||room>65535||frame>65535)throw std::invalid_argument("scene argument range");
        auto files=std::make_shared<re4::port::ImportedFiles>(argv[2],argv[3]);
        re4::port::ControllerInput input;re4::port::GameRuntime game(files,input,static_cast<u8>(disc));game.initialize();
        auto scene=re4::port::loadDiagnosticScene(game.loader(),game.engineConsumers(),static_cast<u16>(room),argv[6],static_cast<u32>(frame));
        std::cout<<scene.report()<<'\n';return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
