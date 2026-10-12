#include "re4/port/gx_scene.h"
#include "re4/port/game_runtime.h"
#include "re4/port/event_skinning.h"
#include "re4/port/motion.h"
#include "re4/port/shelf_callbacks.h"
#include <fstream>
#include <iostream>
#include "gx_verification.h"
int main(int argc,char **argv) {
    try {
        if(argc==2 && std::string(argv[1])=="self-test")return re4::port::verifyGxAdapters();
        if(argc==7 && std::string(argv[1])=="shelf") {
            using namespace re4::port;
            auto disc=std::stoul(argv[4]),room=std::stoul(argv[5],nullptr,16),selection=std::stoul(argv[6]);
            if(disc>1||room>65535||selection>2)throw std::invalid_argument("shelf arguments");
            auto files=std::make_shared<ImportedFiles>(argv[2],argv[3]);ControllerInput input;
            GameRuntime game(files,input,u8(disc));game.initialize();
            std::cout<<verifyShelfCallbacks(game,u16(room),u32(selection))<<'\n';return 0;
        }
        if((argc==4 || argc==6) && std::string(argv[1])=="model") {
            using namespace re4::port;
            auto bytes=[](const char *path){std::ifstream in(path,std::ios::binary|std::ios::ate);auto size=in.tellg();if(!in || size<=0 || size>64*1024*1024)throw std::invalid_argument("skin model fixture size");
                auto b=std::make_shared<std::vector<u8>>(std::size_t(size));in.seekg(0);if(!in.read(reinterpret_cast<char*>(b->data()),size))throw std::runtime_error("skin model fixture read");return ResourceLease{b,b->data(),u32(b->size())};};
            auto bin=bytes(argv[2]),tpl=bytes(argv[3]);if(bin.size<64)throw std::invalid_argument("skin model header");
            EngineConsumers engine;engine.configureModelPools(1,u32(bin.data[25])+2);auto h=engine.initializeModel(bin,tpl,"verified original fixture");auto &cpu=engine.model(h);
            if(argc==6){auto frame=std::stoul(argv[5]);if(frame>65535)throw std::invalid_argument("skin frame");engine.initializeMotion(h,bytes(argv[4]),int(frame));moveModelMotion(cpu);}
            EventSkinSnapshot s;auto pose=captureSkinnedModel(cpu,*cpu.pModelInfo);pose.poseStatus="original bind pose";
            if(argc==6){pose.motion=argv[4];pose.motionFrame=u32(std::stoul(argv[5]));pose.poseStatus="original MotionMove forward kinematics and double joints";s.frame=pose.motionFrame;}
            s.models.push_back(std::move(pose));engine.releaseModel(h);
            if(engine.modelCount())throw std::runtime_error("skin model release");std::cout<<s.report()<<'\n';return 0;
        }
        if(argc==8 && std::string(argv[1])=="skin") {
            auto files=std::make_shared<re4::port::ImportedFiles>(argv[2],argv[3]);re4::port::ControllerInput input;
            auto disc=std::stoul(argv[4]);auto cut=std::stoul(argv[6]);auto frame=std::stoul(argv[7]);if(disc>1 || cut>32767 || frame>65535)throw std::invalid_argument("skin arguments");
            re4::port::GameRuntime game(files,input,u8(disc));game.initialize();
            std::cout<<re4::port::evaluateEventSkinning(game.loader(),game.engineConsumers(),argv[5],u32(cut),u32(frame)).report()<<'\n';return 0;
        }
        if((argc==6||argc==7) && std::string(argv[1])=="scroll") {
            auto disc=std::stoul(argv[4]),room=std::stoul(argv[5],nullptr,16);
            if(disc>1||room>65535)throw std::invalid_argument("scroll arguments");
            auto files=std::make_shared<re4::port::ImportedFiles>(argv[2],argv[3]);re4::port::ControllerInput input;
            re4::port::GameRuntime game(files,input,u8(disc));game.initialize();
            auto first=re4::port::loadOriginalScrollGeometry(game.loader(),game.engineConsumers(),u16(room));
            auto second=re4::port::loadOriginalScrollGeometry(game.loader(),game.engineConsumers(),u16(room));
            if(first.report()!=second.report())throw std::runtime_error("original scroll repeated room/lifetime mismatch");
            if(argc==7) {
                std::ofstream out(argv[6],std::ios::binary|std::ios::trunc);
                out.write(reinterpret_cast<const char*>(second.vertices.data()),std::streamsize(second.vertices.size()*sizeof(re4::port::GxVertex)));
                out.write(reinterpret_cast<const char*>(second.indices.data()),std::streamsize(second.indices.size()*sizeof(u32)));
                if(!out)throw std::runtime_error("private SMD geometry comparison output");
            }
            std::cout<<second.report()<<'\n';return 0;
        }
        if((argc<8 || argc>11) || std::string(argv[1])!="scene")throw std::invalid_argument("scene <root> <catalog> <disc> <room-hex> <event-FST-path> <frame> [event-model] [cut] [debug-view 0..6]; skin <root> <catalog> <disc> <event-FST-path> <cut> <frame>");
        auto disc=std::stoul(argv[4]);auto room=std::stoul(argv[5],nullptr,16);auto frame=std::stoul(argv[7]);
        if(disc>1||room>65535||frame>65535)throw std::invalid_argument("scene argument range");
        auto files=std::make_shared<re4::port::ImportedFiles>(argv[2],argv[3]);
        re4::port::ControllerInput input;re4::port::GameRuntime game(files,input,static_cast<u8>(disc));game.initialize();
        auto cut=argc>=10?std::stoul(argv[9]):UINT32_MAX;if(cut!=UINT32_MAX && cut>32767)throw std::invalid_argument("scene cut");
        auto scene=re4::port::loadDiagnosticScene(game.loader(),game.engineConsumers(),static_cast<u16>(room),argv[6],static_cast<u32>(frame),argc>=9 && std::string(argv[8])!="-"?argv[8]:"",u32(cut),argc==11?u32(std::stoul(argv[10])):0);
        std::cout<<scene.report()<<'\n';return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
