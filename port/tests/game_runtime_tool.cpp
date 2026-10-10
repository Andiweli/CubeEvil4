#include "re4/port/game_runtime.h"
#include "re4/port/sha256.h"
#include "../src/message_original.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <set>
#include <cmath>
namespace {
std::string hash(re4::port::ResourceLease bytes) {
    return re4::port::sha256(bytes.data, bytes.size);
}
// Explicit consumer diagnostics. This never substitutes Event::Init/ExePacket.
void consumers(int argc, char **argv, bool originalEntry=true) {
    using namespace re4::port;
    if (argc < 6)
        throw std::invalid_argument("consumers <root> <catalog> <disc> <FST-event-path>...");
    auto disc = std::stoul(argv[4]);
    if (disc > 1)
        throw std::invalid_argument("disc");
    auto files = std::make_shared<ImportedFiles>(argv[2], argv[3]);
    ControllerInput input;
    GameRuntime game(files, input, static_cast<u8>(disc));
    game.initialize();
    std::shared_ptr<const LoadedFile> player;
    if(originalEntry) {
        auto request=game.loader().request("em/pl00.drs",0x8100);
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
        while(game.loader().status(request)==LoadStatus::Pending || game.loader().status(request)==LoadStatus::Reading) {
            game.tick(); std::this_thread::yield();
            if(std::chrono::steady_clock::now()>deadline) throw std::runtime_error("original pl00 archive read deadline");
        }
        if(game.loader().readCheck(request,player)!=1 || !player) throw std::runtime_error("pl00 original archive read");
    }
    std::cout << "{\"pointer_bits\":" << sizeof(void *) * 8
              << ",\"kind\":\"actual_EVD_BIN_CPU_consumers_not_Event_Init\",\"packages\":[";
    for (int i = 5; i < argc; ++i) {
        auto request = game.loader().request(argv[i]);
        auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (game.loader().status(request) == LoadStatus::Pending ||
               game.loader().status(request) == LoadStatus::Reading) {
            game.tick();
            std::this_thread::yield();
            if (std::chrono::steady_clock::now() > limit)
                throw std::runtime_error("consumer load deadline");
        }
        std::shared_ptr<const LoadedFile> loaded;
        if (game.loader().readCheck(request, loaded) != 1 || !loaded)
            throw std::runtime_error("EVD read failure");
        ResourceLease raw{loaded, loaded->bytes.data(), static_cast<u32>(loaded->bytes.size())};
        auto sourceHash=hash(raw);
        auto &engine = game.engineConsumers();
        if(player) engine.eventResources().bindPlayerArchive({player,player->bytes.data()+player->bodyOffset,player->bodySize});
        auto event = engine.event(raw);
        auto package = engine.event(event).package;
        auto refs=indexEventResourceReferences(package);
        auto findBin=[&](const std::string &name)->const EventBinView* {
            auto it=std::find_if(package.bins.begin(),package.bins.end(),[&](const auto &b){return b.name==name;});
            return it==package.bins.end()?nullptr:&*it;
        };
        auto resource=[&](const std::string &name) { return engine.eventResources().bin(name); };
        u32 diagnosticParts=2;
        for(const auto &ref:refs) if(ref.type!=11) {
            auto bin=resource(ref.bin);
            if(bin.data && bin.size>=0x40) diagnosticParts=std::max(diagnosticParts,u32(bin.data[0x19])+2);
        }
        // Explicit harness capacity, not the production CONS/SMD room count.
        engine.configureModelPools(1,diagnosticParts);
        if (i != 5)
            std::cout << ',';
        std::cout << "{\"path\":\"" << argv[i] << "\",\"sha256\":\"" << hash(raw)
                  << "\",\"room\":\"" << package.room << "\",\"number\":\"" << package.number
                  << "\",\"packet_offset\":" << package.packetOffset
                  << ",\"packet_size\":" << package.packetSize
                  << ",\"player_archive_sha256\":" << std::quoted(player?sha256(player->bytes.data(),player->bytes.size()):"")
                  << ",\"external_bins\":[";
        bool externalComma=false;
        auto &table=engine.eventResources().BinTbl;
        for(int n=0;n<table.NumDatTbl;++n) if((table.pWork[n].FlagBe8&1) && !findBin(table.pWork[n].Name)) {
            auto bytes=resource(table.pWork[n].Name);
            if(externalComma) std::cout << ','; externalComma=true;
            std::cout << "{\"name\":" << std::quoted(table.pWork[n].Name) << ",\"size\":" << bytes.size
                      << ",\"sha256\":" << std::quoted(hash(bytes)) << '}';
        }
        std::cout << "],\"bins\":[";
        std::vector<ConsumerHandle> models;
        for (std::size_t n = 0; n < package.bins.size(); ++n) {
            auto &bin = package.bins[n];
            if (n)
                std::cout << ',';
            std::cout << "{\"name\":" << std::quoted(bin.name)
                      << ",\"offset\":" << (bin.bytes.data - raw.data)
                      << ",\"size\":" << bin.bytes.size << ",\"sha256\":\"" << hash(bin.bytes)
                      << '"';
            if (bin.name.size() >= 4 && bin.name.substr(bin.name.size() - 4) == ".bin") {
                auto handle = engine.model(bin.bytes, bin.name);
                models.push_back(handle);
                auto &model = engine.model(handle);
                model.setNoSuspend(1);
                model.setNoSuspend(0);
                model.updateOldPos();
                std::cout << ",\"original_model_parts\":" << unsigned(model.nParts)
                          << ",\"parents\":[";
                for (u32 k = 0; k < model.nParts; ++k) {
                    if (k)
                        std::cout << ',';
                    auto parent = model.pModelInfo->model_addr->pHead[k].parentNo;
                    auto *expected = parent == 255 ? static_cast<ModelCoord *>(&model)
                                                   : model.getPartsPtr(parent);
                    if (model.getPartsPtr(static_cast<int>(k))->pParent != expected)
                        throw std::runtime_error("actual original model hierarchy differs");
                    std::cout << unsigned(parent);
                }
                std::cout << ']';
            }
            if (bin.name.size() >= 4 && bin.name.substr(bin.name.size() - 4) == ".mdt") {
                engine.bindMessage(1, bin.bytes);
                auto messages = engine.messageCount(1);
                for (int n = 0; n < messages; ++n)
                    if (!engine.message(1, n).data)
                        throw std::runtime_error("original event English message lookup");
                std::cout << ",\"original_english_count\":" << messages;
            }
            std::cout << '}';
        }
        auto count = engine.modelCount();
        std::cout << "],\"model_initializations\":[";
        bool modelComma=false;
        std::set<std::pair<std::string,std::string>> initialized;
        for(const auto &ref:refs) {
            if(ref.type==11 || !initialized.emplace(ref.bin,ref.tpl).second) continue;
            if(modelComma) std::cout << ',';
            modelComma=true;
            std::cout << "{\"bin\":" << std::quoted(ref.bin) << ",\"tpl\":" << std::quoted(ref.tpl);
            EventBinView binView{ref.bin,resource(ref.bin)},tplView{ref.tpl,resource(ref.tpl)};
            auto *bin=&binView,*tpl=&tplView;
            if(!bin->bytes.data || !tpl->bytes.data) { std::cout << ",\"status\":\"external-resource-not-bound\"}"; continue; }
            if(ref.type==4 && !findBin(ref.bin)) {
                std::cout << ",\"status\":\"original-SetPartsSub-in-registration-pass\"}"; continue;
            }
            for(int round=0;round<2;++round) {
                auto handle=engine.initializeModel(bin->bytes,tpl->bytes,ref.bin);
                auto &model=engine.model(handle); auto *d=model.pModelInfo->model_addr;
                auto *texture=static_cast<TexturePaletteView*>(model.pModelInfo->tpl_addr);
                if(!d->resolved || !texture->resolved || model.nParts!=bin->bytes.data[0x19] ||
                   model.Motion.Seq_speed!=1 || !(model.be_flag&2)) throw std::runtime_error("original modelInit state");
                for(u32 p=0;p<model.nParts;++p) {
                    auto *part=model.getPartsPtr(int(p)); auto parent=d->pHead[p].parentNo;
                    auto *expected=parent==255?static_cast<ModelCoord*>(&model):model.getPartsPtr(parent);
                    if(part->pParent!=expected || !std::isfinite(part->world.x) || !std::isfinite(part->world.y) ||
                       !std::isfinite(part->world.z) || part->world.x!=part->world_old.x ||
                       part->lt_inv_mat[0][3]!=-part->world.x) throw std::runtime_error("original model matrices/references");
                }
                auto read32=[&](u32 p) { const auto *b=bin->bytes.data+p; return (u32(b[0])<<24)|(u32(b[1])<<16)|(u32(b[2])<<8)|b[3]; };
                auto read16=[&](u32 p) { const auto *b=bin->bytes.data+p; return u16((u16(b[0])<<8)|b[1]); };
                if(d->version==0x20030818) {
                    auto flip=read32(0x44),blend=read32(0x40);
                    if(bool(model.pXFlip)!=bool(flip) || bool(model.pDblJnt)!=bool(blend)) throw std::runtime_error("original motion table assignment");
                    for(u32 p=0;flip && p<model.nParts;++p)
                        if(model.pXFlip[p]!=read16(flip+4+p*2)) throw std::runtime_error("native flip joint endian/reference");
                    if(blend) {
                        u32 count; std::memcpy(&count,model.pDblJnt,4);
                        if(count!=read32(blend) || reinterpret_cast<std::uintptr_t>(model.pDblJnt)%4) throw std::runtime_error("native blend header alignment/endian");
                        for(u32 p=0;p<count*4;++p) if(model.pDblJnt[2+p]!=read16(blend+4+p*2)) throw std::runtime_error("native blend record endian/reference");
                    }
                }
                if(!round) std::cout << ",\"parts\":" << unsigned(model.nParts) << ",\"textures\":" << texture->numDescriptors
                                    << ",\"vertices\":" << d->nVtx << ",\"bounding_box\":["
                                    << model.pModelInfo->bound.offset.x << ',' << model.pModelInfo->bound.offset.y << ','
                                    << model.pModelInfo->bound.offset.z << ',' << model.pModelInfo->bound.size.x << ','
                                    << model.pModelInfo->bound.size.y << ',' << model.pModelInfo->bound.size.z << ']';
                engine.releaseModel(handle);
                try { engine.model(handle); throw std::runtime_error("model handle release failed"); }
                catch(const std::logic_error &) {}
            }
            std::cout << ",\"status\":\"original-modelInit-two-rounds-released\"}";
        }
        std::cout << "],\"event_registration\":[";
        // Execute SetMod/GetMod on CPU models, retaining all bindings through motion
        // initialization. This does not claim ExePacket_SetOm/SetObj18 cloth/light.
        engine.clear();
        if(player) engine.eventResources().bindPlayerArchive({player,player->bytes.data()+player->bodyOffset,player->bodySize});
        event=engine.event(raw);
        u32 totalParts=2,modelSlots=0;
        std::set<std::string> names;
        for(const auto &ref:refs) if(ref.type!=11 && names.insert(ref.model).second) {
            auto bin=resource(ref.bin),tpl=resource(ref.tpl);
            if(bin.data && tpl.data) { if(ref.type==3) totalParts+=bin.data[0x19]; ++modelSlots; }
        }
        engine.configureModelPools(std::max(u32(1),modelSlots),totalParts);
        std::map<std::string,ConsumerHandle> registered;
        bool comma=false;
        for(const auto &ref:refs) if(ref.type==3 && !registered.count(ref.model)) {
            auto bin=resource(ref.bin),tpl=resource(ref.tpl);
            if(!bin.data || !tpl.data) continue;
            auto h=engine.initializeModel(bin,tpl,ref.bin);
            engine.registerEventModel(event,h,ref.model,2); registered.emplace(ref.model,h);
            void *found=nullptr; u8 type=0; int slot=-1;
            if(!engine.event(event).GetMod(&found,ref.model.c_str(),&type,&slot) || found!=&engine.model(h) || type!=2)
                throw std::runtime_error("original Event SetMod/GetMod native pointer identity");
            if(comma) std::cout << ','; comma=true;
            std::cout << "{\"name\":" << std::quoted(ref.model) << ",\"type\":" << unsigned(type) << ",\"slot\":" << slot << '}';
        }
        std::set<std::string> attachments;
        for(const auto &ref:refs) if(ref.type==4 && attachments.insert(ref.model).second) {
            if(!registered.count(ref.parent)) throw std::runtime_error("original attachment parent missing");
            if(!engine.event(event).ExePacket_SetPartsSub(ref.model.c_str(),ref.bin.c_str(),ref.tpl.c_str(),ref.parent.c_str()))
                throw std::runtime_error("actual original SetPartsSub failed");
            void *found=nullptr; u8 type=0; int slot=-1;
            if(!engine.event(event).GetMod(&found,ref.model.c_str(),&type,&slot) || !found || type!=3)
                throw std::runtime_error("original SetPartsSub info registration");
            auto *info=static_cast<ModelInfoView*>(found);
            if(comma) std::cout << ','; comma=true;
            std::cout << "{\"name\":" << std::quoted(ref.model) << ",\"parent\":" << std::quoted(ref.parent)
                      << ",\"type\":3,\"slot\":" << slot << ",\"parts\":" << unsigned(info->model_addr->nParts) << '}';
        }
        std::cout << "],\"motion_initializations\":["; comma=false;
        for(const auto &ref:refs) if(ref.type==11 && registered.count(ref.model)) {
            auto motion=resource(ref.bin);
            if(!motion.data) throw std::runtime_error("original registered motion reference missing");
            // ExePacket_Mot explicitly skips a zero maxFrame word.
            if(!motion.data[0] && !motion.data[1]) continue;
            auto h=registered.at(ref.model);
            engine.initializeMotion(h,motion);
            const auto &w=engine.model(h).Motion;
            if(!w.pMot || w.Joint_num!=motion.data[2] || !(w.Mot_flag&0x04000000))
                throw std::runtime_error("actual original MotionSetCore state");
            if(comma) std::cout << ','; comma=true;
            std::cout << "{\"model\":" << std::quoted(ref.model) << ",\"bin\":" << std::quoted(ref.bin)
                      << ",\"joints\":" << unsigned(w.Joint_num) << ",\"max_frame\":" << w.Mot_frame_max
                      << ",\"null_pos\":" << w.Null_pos << ",\"null_rot\":" << w.Null_rot
                      << ",\"root\":[" << w.Pos.x << ',' << w.Pos.y << ',' << w.Pos.z << "],\"samples\":[";
            for(int sample=0;sample<3;++sample) {
                if(sample) std::cout << ',';
                auto v=engine.sampleMotion(h,0,w.Mot_frame_max*float(sample)/2);
                std::cout << '[' << v.x << ',' << v.y << ',' << v.z << ']';
            }
            std::cout << "]}";
        }
        std::cout << "],\"player_motion_initializations\":["; comma=false;
        if(player) {
            auto body=ResourceLease{player,player->bytes.data()+player->bodyOffset,player->bodySize};
            auto index=indexAssetBody(body.data,body.size);
            // Use the model already bound by the original EVD SetOm reference to
            // pl000a.bin; select FCV entries by the original archive index/tag.
            auto ref=std::find_if(refs.begin(),refs.end(),[](const auto &r) {
                return r.type==3 && r.bin=="em/pl00/pl000a.bin";
            });
            if(ref==refs.end()) throw std::runtime_error("original opening player model reference absent");
            auto h=registered.at(ref->model);
            for(const auto &entry:index.entries) if(!std::memcmp(entry.tag.data(),"FCV",3)) {
                ResourceLease motion{body.owner,body.data+entry.offset,entry.size};
                engine.initializeMotion(h,motion);
                auto &w=engine.model(h).Motion;
                if(!w.pMot || w.Joint_num!=motion.data[2]) throw std::runtime_error("player original MotionSetCore state");
                if(comma) std::cout << ','; comma=true;
                std::cout << "{\"index\":" << entry.index << ",\"sha256\":" << std::quoted(hash(motion))
                          << ",\"joints\":" << unsigned(w.Joint_num) << ",\"max_frame\":" << w.Mot_frame_max
                          << ",\"null_pos\":" << w.Null_pos << ",\"null_rot\":" << w.Null_rot << ",\"samples\":[";
                for(int sample=0;w.Joint_num && sample<3;++sample) {
                    if(sample) std::cout << ',';
                    auto v=engine.sampleMotion(h,0,w.Mot_frame_max*float(sample)/2);
                    std::cout << '[' << v.x << ',' << v.y << ',' << v.z << ']';
                    // Decode every joint stream with the actual original curve
                    // readers, not only the independently reported first joint.
                    for(u32 joint=1;joint<w.Joint_num;++joint) {
                        auto value=engine.sampleMotion(h,joint,w.Mot_frame_max*float(sample)/2);
                        if(!std::isfinite(value.x)||!std::isfinite(value.y)||!std::isfinite(value.z))
                            throw std::runtime_error("player motion nonfinite original sample");
                    }
                }
                std::cout << "]}";
            }
        }
        std::cout << "],\"event_resource_references\":[";
        for(std::size_t n=0;n<refs.size();++n) {
            if(n) std::cout << ',';
            const auto &r=refs[n];
            std::cout << "{\"offset\":" << r.packetOffset << ",\"type\":" << r.type << ",\"model\":" << std::quoted(r.model)
                      << ",\"parent\":" << std::quoted(r.parent) << ",\"bin\":" << std::quoted(r.bin)
                      << ",\"tpl\":" << std::quoted(r.tpl) << ",\"bin_in_package\":" << (findBin(r.bin)?"true":"false") << '}';
        }
        if (package.room.size() != 4 || package.room[0] != 'r')
            throw std::runtime_error("original EVD room identity");
        auto roomId = static_cast<u16>(std::stoul(package.room.substr(1), nullptr, 16));
        auto &catalog = originalModuleCatalog();
        auto metadata = std::find_if(catalog.rooms.begin(), catalog.rooms.end(),
                                     [&](const auto &r) { return r.id == roomId; });
        if (metadata == catalog.rooms.end())
            throw std::runtime_error("uncatalogued EVD room");
        std::string blocker;
        if(originalEntry) {
            auto activated = game.queueModule(ModuleOperation::Activate, metadata->module);
            game.tick();
            if (activated->status != OperationStatus::Complete) throw std::runtime_error(activated->error);
            try { game.invokeRoomEntry(roomId,true); }
            catch(const MissingGameConsumer &failure) { blocker=failure.what(); }
            if(!blocker.empty() || game.texRender().count()!=1)
                throw std::runtime_error("original R120Init must complete CPU initialization");
        }
        auto before = game.state().Scenario_flg[1];
        if(originalEntry) engine.invoke(event, 3, 0); // Original cancel callback, not packet execution.
        auto after = game.state().Scenario_flg[1];
        if(originalEntry) { game.queueRoomRetirement(roomId); game.tick(); }
        else engine.clear(); // Explicit consumer diagnostic, no other-room entry claimed.
        bool stale = false;
        try {
            engine.event(event);
        } catch (const std::logic_error &) {
            stale = true;
        }
        if (!stale || engine.modelCount())
            throw std::runtime_error("consumer resource retirement");
        for (auto handle : models) {
            stale = false;
            try {
                engine.model(handle);
            } catch (const std::logic_error &) {
                stale = true;
            }
            if (!stale)
                throw std::runtime_error("stale model reference accepted");
        }
        std::cout << "],\"native_models\":" << count
                  << ",\"retired\":true,\"original_init_blocker\":" << std::quoted(blocker)
                  << ",\"callback_mode\":" << (originalEntry?3:-1) << ",\"flags_before\":" << before
                  << ",\"flags_after\":" << after << '}';
        if(hash(raw)!=sourceHash) throw std::runtime_error("model initialization mutated original immutable bytes");
    }
    std::cout << "]}\n";
}
void data(int argc, char **argv) {
    using namespace re4::port;
    if (argc < 6)
        throw std::invalid_argument("data <root> <catalog> <disc> <room-hex>...");
    auto disc = std::stoul(argv[4]);
    if (disc > 1)
        throw std::invalid_argument("disc");
    auto files = std::make_shared<ImportedFiles>(argv[2], argv[3]);
    ControllerInput input;
    GameRuntime game(files, input, static_cast<u8>(disc));
    game.initialize();
    std::cout << "{\"pointer_bits\":" << sizeof(void *) * 8
              << ",\"kind\":\"shared_runtime_original_archive_and_esl_reads\",\"rooms\":[";
    for (int i = 5; i < argc; ++i) {
        auto id = std::stoul(argv[i], nullptr, 16);
        if (id > UINT16_MAX)
            throw std::invalid_argument("room ID");
        game.state().room_id = static_cast<u16>(id);
        game.state().em_list_no = -1;
        std::fill(std::begin(game.state().Em_list), std::end(game.state().Em_list), 0x71);
        auto index = game.enemyListNumber(static_cast<u16>(id));
        auto name = game.enemyListName(static_cast<u32>(index));
        auto *identity = index >= 0 ? files->find(static_cast<u8>(disc), name) : nullptr;
        if (!identity || identity->size > sizeof(game.state().Em_list))
            throw std::runtime_error("original ESL identity/range");
        bool listReady = false;
        auto task = game.loader().scheduler().exec(1, [&] {
            game.readEnemyList();
            listReady = true;
            game.loader().scheduler().exit();
        });
        if (!task || !game.loader().loadRoom(static_cast<u16>(id)))
            throw std::runtime_error("shared runtime load start");
        auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (!listReady || game.loader().roomBusy()) {
            game.tick();
            std::this_thread::yield();
            if (std::chrono::steady_clock::now() > limit)
                throw std::runtime_error("shared runtime data deadline");
        }
        auto room = game.loader().room();
        if (!room)
            throw std::runtime_error(game.loader().roomError());
        auto eslHash = sha256(game.state().Em_list, identity->size);
        if (eslHash != identity->sha256)
            throw std::runtime_error("actual original readEmList destination hash mismatch");
        for (auto j = identity->size; j < sizeof(game.state().Em_list); ++j)
            if (game.state().Em_list[j] != 0x71)
                throw std::runtime_error("ESL short read overwrote tail");
        if (i != 5)
            std::cout << ',';
        std::cout << "{\"id\":\"" << argv[i] << "\",\"source_sha256\":\""
                  << sha256(room->source->bytes.data(), room->source->bytes.size())
                  << "\",\"body_sha256\":\"" << sha256(room->body->data(), room->body->size())
                  << "\",\"body_size\":" << room->body->size() << ",\"esl_path\":\"" << name
                  << "\",\"esl_size\":" << identity->size << ",\"esl_sha256\":\"" << eslHash
                  << "\",\"entries\":" << room->index.entries.size() << ",\"blocks\":[";
        for (std::size_t n = 0; n < room->blocks.size(); ++n) {
            if (n)
                std::cout << ',';
            auto &block = *room->blocks[n];
            std::cout << "{\"path\":\"" << block.identity.path << "\",\"sha256\":\""
                      << sha256(block.bytes.data(), block.bytes.size()) << "\"}";
        }
        std::cout << "],\"original_read_observed\":"
                  << (std::find(game.trace().begin(), game.trace().end(), "DvdReadN:" + name) !=
                              game.trace().end()
                          ? "true"
                          : "false")
                  << ",\"consumers\":[";
        bool comma = false;
        auto &engine = game.engineConsumers();
        for (const auto &entry : room->index.entries) {
            if (std::memcmp(entry.tag.data(), "CAM", 3) && std::memcmp(entry.tag.data(), "MDT", 3))
                continue;
            ResourceLease span{room->body, room->body->data() + entry.offset, entry.size};
            if (comma)
                std::cout << ',';
            comma = true;
            std::cout << "{\"index\":" << entry.index << ",\"tag\":\""
                      << std::string(entry.tag.begin(), entry.tag.begin() + 3) << "\",\"sha256\":\""
                      << hash(span) << '"';
            try {
                if (!std::memcmp(entry.tag.data(), "CAM", 3)) {
                    engine.bindCamera(span);
                    const auto &cam = engine.camera();
                    std::cout << ",\"status\":\"indexed\",\"version\":" << unsigned(cam.version)
                              << ",\"areas\":" << unsigned(cam.areas)
                              << ",\"cameras\":" << unsigned(cam.cameras);
                } else {
                    engine.bindMessage(1, span);
                    auto count = engine.messageCount(1);
                    for (int n = 0; n < count; ++n)
                        if (!engine.message(1, n).data)
                            throw std::runtime_error("original message lookup");
                    std::cout << ",\"status\":\"indexed\",\"count\":" << count;
                }
            } catch (const std::invalid_argument &failure) {
                std::cout << ",\"status\":\"open\",\"reason\":\"" << failure.what() << '"';
            }
            std::cout << '}';
        }
        engine.clear();
        std::cout << "]}";
    }
    std::string blocker;
    try {
        game.scenarioRoomInit();
    } catch (const MissingGameConsumer &e) {
        blocker = e.what();
    }
    if (blocker.empty())
        throw std::runtime_error("missing shared scenario consumers must not succeed");
    std::cout << "],\"scenario_blocker\":\"" << blocker
              << "\",\"frames\":" << game.state().Frame_cnt << "}\n";
}
} // namespace
// Actual shared boot consumers with private disc bytes, deliberately stops at
// the next missing original game dependency rather than inventing its state.
void messages(int argc,char **argv) {
    using namespace re4::port;
    if(argc!=5) throw std::invalid_argument("messages <root> <catalog> <disc>");
    auto disc=std::stoul(argv[4]); if(disc>1) throw std::invalid_argument("disc");
    auto files=std::make_shared<ImportedFiles>(argv[2],argv[3]);
    ControllerInput input; GameRuntime game(files,input,static_cast<u8>(disc)); game.initialize();
    auto &m=game.messages(); auto &scheduler=game.loader().scheduler();
    int checks=0;
    auto check=[&](bool yes,const char *why){ ++checks; if(!yes) throw std::runtime_error(why); };
    auto rejects=[&](const TaskEntry &call){ bool no=false; try{call();}catch(const std::exception &){no=true;} check(no,"invalid message input was accepted"); };
    auto execute=[&](const TaskEntry &call){
        bool done=false; auto task=scheduler.exec(1,[&]{call();done=true;scheduler.exit();});
        check(bool(task),"message task creation");
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
        while(!done){game.tick();std::this_thread::yield();if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("message boot timeout");}
    };
    auto cancelled=scheduler.exec(1,[&]{m.initialize();scheduler.exit();});
    check(bool(cancelled),"cancelled boot creation");
    check(scheduler.suspend(*cancelled),"suspend original boot before dispatch"); game.tick();
    check(!m.ready(),"suspended boot was dispatched");
    check(scheduler.signal(*cancelled),"resume original boot before dispatch");
    check(scheduler.kill(*cancelled),"cancel original boot before dispatch"); game.tick();
    check(!m.ready() && game.state().IsMessageInit==0,"cancelled boot falsely complete");
    game.initialize();
    execute([&]{m.initialize();});
    check(m.ready(),"original MessageControl::init did not complete");
    auto core=m.coreBody(); auto coreHash=hash(core); auto textures=m.coreTextures(); auto thermal=m.thermalPalette();
    check(!textures.empty() && thermal.initialized,"original core texture consumers missing");
    check(scheduler.allocatedStacks()==0,"message boot stack remains active");
    auto &ids=game.ids(); auto &saves=game.saveState();
    auto preHud=ids.pull(); check(bool(preHud),"original ID pool prerequisite");
    check(bool(game.startOriginalGameTask()),"original GameTask creation");
    std::string blocker;
    try{game.tick();}catch(const MissingGameConsumer &failure){blocker=failure.what();}
    check(blocker=="Original consumer unavailable: LightMgr.init(LightFuncTbl)","actual GameTask must pass original Cockpit/ID initialization and stop at lights");
    rejects([&]{(void)ids.unit(*preHud);}); // Actual Cockpit::gameInit invalidates the old HUD unit.
    const auto idBoot=ids.snapshot(); const auto saveBoot=saves.snapshot();
    check(idBoot.initialized && idBoot.capacity==0x80 && idBoot.alive==0,"actual original ID boot/reset pool");
    check(std::all_of(idBoot.setFlags.begin(),idBoot.setFlags.end(),[](u32 f){return !f;}) &&
          std::all_of(idBoot.displayOff.begin(),idBoot.displayOff.end(),[](u32 f){return !f;}),"original ID room flag clearing");
    check(std::count(ids.trace().begin(),ids.trace().end(),"void Cockpit::gameInit()") == 1 &&
          std::count(ids.trace().begin(),ids.trace().end(),"void IDSystem::roomInit()") == 2,"real GameTask Cockpit/ID source order");
    check(saveBoot.initialized && saveBoot.rooms==216 && saveBoot.savedRooms==saveBoot.roomIds.size() &&
          saveBoot.roomBytes==16+216*saveBoot.savedRooms,"original room save table initialization");
    auto &ss=game.subscreen(); auto sw=ss.snapshot(); auto sr=ss.resources();
    check(sw.initialized && sw.filename=="SS/eng/ss_pzzl.dat" && sw.moduleAram==0xD00000,"actual original SubScreen language/ARAM state");
    check(sw.boardSize==0 && sw.boardNext==0 && sw.mapMode==0 && sw.open==0 && sw.attributes==0 && sw.close==0 && sw.wait==0 && sw.mapArrayEmpty,"actual original SubScreen/MapMgr initialization");
    check(game.state().ope_mdt_no==0x18 && game.state().ope_x82E8==0 && game.state().ope_mdt_bits[0]==0,"original Ope state clear");
    check((game.state().Status_flg[STA_SSCRN_ENABLE/32] & (0x80000000u>>(STA_SSCRN_ENABLE%32)))!=0,"original SubScreen enable flag");
    check(sr.size()==3 && ss.itemCapacity()==0x180 && !ss.findItem(0x7c),"real Title_task inventory boot prerequisite and three ARAM reads");
    u32 aram=0xD00000;
    for(auto &r:sr){check(r.aramAddress==aram && r.dmaBytes==((r.identity.size+31u)&~31u),"original rounded DMA offsets");check(hash(r.bytes)==r.identity.sha256,"actual ARAM to MRAM readback differs from FST hash"); aram+=r.dmaBytes;}
    check(sw.freeOffset==aram-0xD00000 && sw.commonOffset==sr[0].dmaBytes && sw.puzzleOffset==sr[0].dmaBytes+sr[1].dmaBytes,"original ARAM archive offsets");
    check(game.state().IsMessageInit==1 && game.state().Rno0==0,"original message flag or uncompleted gameplay state");
    auto font=m.font(0),system=m.font(1);
    check(font.loaded && system.loaded && font.charWidth==32 && font.charHeight==32,"actual English font slots");
    check(font.textures.size()==system.textures.size() && font.widths.data==system.widths.data,"original English system-font alias");
    for(int n=0;n<16;++n) check(m.layout(n).queueBound==(n<3),"original glyph queue registration");
    std::cout<<"{\"pointer_bits\":"<<sizeof(void*)*8<<",\"kind\":\"actual_CoreDataRead_MessageControl_init_gameInit_Font_create\",\"core_body_sha256\":"<<std::quoted(coreHash)<<",\"core_textures\":[";
    for(std::size_t i=0;i<textures.size();++i){if(i)std::cout<<',';auto &t=textures[i];std::cout<<"{\"width\":"<<t.width<<",\"height\":"<<t.height<<",\"format\":"<<t.format<<",\"sha256\":"<<std::quoted(hash(t.pixels))<<'}';}
    std::cout<<"],\"thermal_sha256\":"<<std::quoted(hash(thermal.entries))<<",\"font_textures\":[";
    for(std::size_t i=0;i<font.textures.size();++i){if(i)std::cout<<',';auto &t=font.textures[i];auto &p=font.palettes[i];
        check(t.initialized && t.indexed && t.minFilter==1 && t.magFilter==1 && p.initialized,"original GX font CPU state");
        std::cout<<"{\"width\":"<<t.width<<",\"height\":"<<t.height<<",\"format\":"<<t.format<<",\"sha256\":"<<std::quoted(hash(t.pixels))<<",\"palette_entries\":"<<p.count<<",\"palette_format\":"<<p.format<<",\"palette_sha256\":"<<std::quoted(hash(p.entries))<<'}';}
    std::cout<<"],\"widths_sha256\":"<<std::quoted(hash(font.widths))<<",\"glyphs\":[";
    for(u32 n=0;n<font.widths.size/2;++n){s8 l{},r{};auto w=m.glyphWidth(0,int(n),&l,&r);auto at=(n?n:1)*2;
        check(l==s8(font.widths.data[at]) && r==s8(font.widths.data[at+1]) && w==r-l,"actual original glyph bearings");
        if(n)std::cout<<',';std::cout<<'['<<int(l)<<','<<int(r)<<','<<w<<']';}
    std::cout<<"],\"layouts\":[";
    for(int n=0;n<9;++n){m.setLayout(0,n);auto l=m.layout(0);if(n)std::cout<<',';std::cout<<'['<<int(l.width)<<','<<int(l.height)<<','<<l.charGap<<','<<l.lineGap<<']';}
    std::cout<<"],\"messages\":[";
    for(int type=0;type<4;++type){if(type)std::cout<<',';int count=m.messageCount(type); std::cout<<"{\"type\":"<<type<<",\"count\":"<<count<<",\"payload_hashes\":[";
        for(int n=0;n<count;++n){auto raw=m.message(type,n);auto native=m.nativeMessageWords(type,n);bool valid=true;
            for(std::size_t k=0;k<native.size();++k) valid=valid && native[k]==u16(u16(raw.data[2*k])<<8|raw.data[2*k+1]);
            check(valid && m.firstNativeWord(type,n)==native[0],"native message BE words/reference");if(n)std::cout<<',';std::cout<<std::quoted(hash(raw));}
        std::cout<<"]}";
    }
    rejects([&]{m.setLayout(-1,0);}); rejects([&]{m.setLayout(0,9);}); rejects([&]{m.glyphWidth(0,-1,nullptr,nullptr);});
    rejects([&]{s8 l,r;m.glyphWidth(0,int(font.widths.size/2),&l,&r);}); rejects([&]{m.message(0,-1);}); rejects([&]{m.message(0,m.messageCount(0));});
    rejects([&]{resolveMessageTpl({font.tplSource.owner,font.tplSource.data,11},2);});
    auto mutated=std::make_shared<std::vector<u8>>(font.tplSource.data,font.tplSource.data+font.tplSource.size);
    ResourceLease bad{mutated,mutated->data(),static_cast<u32>(mutated->size())};
    (*mutated)[7]=3; rejects([&]{resolveMessageTpl(bad,2);}); // Original FONT_TEX capacity is two.
    std::memcpy(mutated->data(),font.tplSource.data,font.tplSource.size);
    (*mutated)[8]=0xff; rejects([&]{resolveMessageTpl(bad,2);}); // BE descriptor offset outside file.
    std::memcpy(mutated->data(),font.tplSource.data,font.tplSource.size);
    auto table=u32((*mutated)[8])<<24|u32((*mutated)[9])<<16|u32((*mutated)[10])<<8|(*mutated)[11];
    for(int k=4;k<8;++k) (*mutated)[table+u32(k)]=0;
    rejects([&]{resolveMessageTpl(bad,2);}); // Indexed font without CLUT.
    execute([&]{rejects([&]{m.clear();});}); // Live task cannot invalidate another stack's views.
    auto widthHash=hash(font.widths); auto pixelHash=hash(font.textures[0].pixels);
    m.releaseFont(0); check(!m.font(0).loaded && m.font(1).loaded,"font release corrupts alias slot");
    check(hash(system.widths)==widthHash && hash(system.textures[0].pixels)==pixelHash,"shared original font data lost");
    execute([&]{m.reloadCommonFont();}); check(m.font(0).loaded && hash(m.font(0).widths)==widthHash,"font reinitialization");
    // Isolated tests of the actual original ID pool bodies. These unit fields
    // exercise hierarchy and lifetimes; no fake HUD is attached to gameplay.
    auto parent=ids.pull(),child=ids.pull(); check(parent && child,"original ID unitPull");
    ids.unit(*parent).type=1; ids.parent(*parent,*child);
    check(ids.unit(*child).pParent==&ids.unit(*parent) && ids.unit(*child).levelNo==1,"original ID parent/depth");
    rejects([&]{ids.parent(*child,*parent);});
    ids.time(*parent,123); ids.moving(*parent,false);
    check(ids.unit(*parent).timer[0]==123 && ids.unit(*child).timer[3]==123 && !(ids.unit(*child).be_flag&4),"original recursive ID time/move flags");
    ids.moving(*parent,true); check((ids.unit(*parent).be_flag&4) && (ids.unit(*child).be_flag&4),"original recursive ID resume");
    ids.display(IDC_LIFE_METER,0); check(ids.snapshot().displayOff[1]==0x40000000u,"original class 33 display bit");
    ids.roomInit(); rejects([&]{(void)ids.unit(*parent);}); rejects([&]{(void)ids.unit(*child);});
    std::vector<IdHandle> filled;
    for(int n=0;n<0x80;++n){ auto h=ids.pull(); check(bool(h),"original ID sequential pool allocation"); auto &u=ids.unit(*h);
        check(h->slot==u32(n) && u.be_flag==0xD && !u.pParent && !u.path0 && !u.path1 && !u.curve[0] &&
              u.u0==0 && u.u1==1 && u.v0==0 && u.v1==1 && u.timer[0]==0,"original cleared native ID fields/UV"); filled.push_back(*h); }
    check(!ids.pull() && ids.snapshot().alive==128,"original ID pool exhaustion");
    ids.push(filled[3]); rejects([&]{(void)ids.unit(filled[3]);});
    auto reused=ids.pull(); check(reused && reused->slot==3 && reused->generation!=filled[3].generation,"ID generation on original slot reuse");
    ids.kill(0xFF,IDC_ANY); check(ids.snapshot().alive==0,"original ID kill all");
    ids.roomInit(); parent=ids.pull(); child=ids.pull(); ids.unit(*parent).type=1; ids.parent(*parent,*child);
    ids.push(*parent); check(ids.snapshot().alive==0,"original group release recursively frees child"); rejects([&]{(void)ids.unit(*child);});
    {
        GameplayHeaps isolatedHeaps;
        IdRuntime a(isolatedHeaps,scheduler),b(isolatedHeaps,scheduler);
        a.initialize(); b.initialize(); auto ha=a.pull(),hb=b.pull();
        check(ha && hb && ha->slot==hb->slot && ha->generation==hb->generation && ha->runtime!=hb->runtime,"isolated original ID pools with equal slot generations");
        rejects([&]{(void)a.unit(*hb);}); rejects([&]{(void)b.unit(*ha);});
        a.clear(); check(b.unit(*hb).be_flag==0xD,"freeing one original pool preserves the other"); b.clear();
    }
    execute([&]{rejects([&]{ids.clear();});rejects([&]{saves.clear();});});
    // Actual room-save and cGameSave pointer bodies; no aggregate global/player
    // save/load is claimed. Native images never masquerade as GC card bytes.
    auto nativeImage=saves.image();
    for(auto room:saveBoot.roomIds){ check(!saves.passed(room,3),"original blank room record"); saves.setPassed(room,3); check(saves.passed(room,3)==0x10,"original room passed bit"); }
    saves.saveRooms(nativeImage);
    for(auto room:saveBoot.roomIds) saves.setPassed(room,5);
    saves.loadRooms(nativeImage);
    for(auto room:saveBoot.roomIds) check(saves.passed(room,3)==0x10 && !saves.passed(room,5),"original room state restored by ID");
    auto nativeCopy=saves.copyImage(nativeImage); check(nativeCopy.head->base==nativeCopy.head && nativeCopy.head!=nativeImage.head,"original checkAddr rebases copied native image");
    saves.offsets(nativeCopy); check(!nativeCopy.head->base && reinterpret_cast<std::uintptr_t>(nativeCopy.head->pGlobal)==0x40,"original save offset form");
    saves.offsets(nativeCopy); saves.addresses(nativeCopy); saves.addresses(nativeCopy);
    check(reinterpret_cast<u8*>(nativeCopy.head->pItm)-reinterpret_cast<u8*>(nativeCopy.head)==saveBoot.sectionOffsets[4],"native-width calcAddr preserves original section offsets");
    auto *roomHdr=static_cast<RoomSaveHdr*>(nativeCopy.head->pRm); auto num=roomHdr->num; roomHdr->num=UINT32_MAX;
    rejects([&]{saves.loadRooms(nativeCopy);}); roomHdr->num=num;
    auto rm=nativeCopy.head->pRm; nativeCopy.head->pRm=nullptr; rejects([&]{saves.offsets(nativeCopy);}); nativeCopy.head->pRm=rm;
    rejects([&]{saves.setPassed(0x120,8);});
    saves.clearRooms(nativeCopy); check(saves.snapshot().roomIds==saveBoot.roomIds,"original room clear preserves room IDs");
    for(auto room:saveBoot.roomIds) check(!saves.passed(room,3),"original room clear removed flags");
    check(!saves.passed(0x000,0) && !saves.passed(0xFFF,0),"original room without save record");
    auto idTrace=ids.trace(); auto saveTrace=saves.trace();
    auto trace=m.trace(); auto ssTrace=ss.trace(); game.initialize();
    rejects([&]{saves.addresses(nativeCopy);}); check(nativeCopy.head->base==nativeCopy.head,"retired native save lease remains owned");
    check(ids.snapshot().capacity==128 && saves.snapshot().roomIds==saveBoot.roomIds,"repeated original ID/room/save initialization");
    check(!m.ready() && !m.font(1).loaded && game.state().IsMessageInit==0,"native/original reset release");
    check(hash(font.widths)==widthHash && hash(core)==coreHash,"retired snapshot lease lost");
    execute([&]{m.initialize();m.gameInit();}); check(m.messageCount(0)>0 && m.font(1).loaded,"repeated original boot");
    m.clear(); check(!m.ready() && !m.font(0).loaded && scheduler.allocatedStacks()==0,"message final release");
    // Isolated original inventory function checks. These entries are never
    // presented as the game's start inventory or inserted into GameTask.
    for(u16 id=0x7c;id<=0x7f;++id){
        check(ss.getItem(id,1)==1,"original item registration"); auto *item=ss.findItem(id);
        check(item && item->id==id && item->num==1 && ss.itemSlot(item)>=0,"original item identity/slot");
        ss.roomInit(); check(ss.snapshot().boardSize==id-0x7c,"original case-size lookup");
    }
    game.state().pl_type=1; ss.inventoryRoomInit(); ss.roomInit();
    check(!ss.findItem(0x7c) && ss.snapshot().boardSize==0,"original Ashley inventory set");
    game.state().pl_type=0; ss.inventoryRoomInit(); ss.roomInit(); check(ss.snapshot().boardSize==3,"original Leon inventory set preserved");
    ss.wait(7); check(ss.snapshot().wait==7,"original SubScreenWait"); ss.roomInit(); check(ss.snapshot().wait==0,"original room reset");
    ss.clearInventory(); check(!ss.findItem(0x7f),"original inventory clear");
    check(ss.getItem(0x23)==1 && ss.findItem(0x23)->bullet==10,"original handgun construct and weapon-table magazine");
    check(ss.getItem(0x57)==1 && ss.getItem(0x57)==1 && ss.findItem(0x57)->num==2,"original treasure default and stacking table");
    check(ss.getItem(0xac,1)==1 && ss.findItem(0xac)->lv8[0]==0,"original document byte field/countFiles");
    ss.clearInventory();
    execute([&]{ss.gameInit();}); check(ss.snapshot().initialized,"repeated original SubScreenGameInit");
    rejects([&]{game.loader().request("rel/Sscrn.rel",9,4,0,0,0xD00001);});
    rejects([&]{game.loader().readAram(0xFFFFE0,64);});
    std::cout<<"],\"subscreen\":{\"filename\":"<<std::quoted(sw.filename)<<",\"free_offset\":"<<sw.freeOffset<<",\"common_offset\":"<<sw.commonOffset<<",\"puzzle_offset\":"<<sw.puzzleOffset<<",\"module_aram\":"<<sw.moduleAram<<",\"inventory_slots\":"<<ss.itemCapacity()<<",\"resources\":[";
    for(std::size_t i=0;i<sr.size();++i){if(i)std::cout<<',';auto &r=sr[i];std::cout<<"{\"path\":"<<std::quoted(r.identity.path)<<",\"size\":"<<r.bytes.size<<",\"sha256\":"<<std::quoted(hash(r.bytes))<<",\"aram\":"<<r.aramAddress<<",\"dma_bytes\":"<<r.dmaBytes<<'}';}
    std::cout<<"],\"item_info\":[";
    for(u16 id=0;id<256;++id){auto info=ss.itemInfo(id);if(id)std::cout<<',';std::cout<<'['<<info.id<<','<<unsigned(info.type)<<','<<unsigned(info.defNum)<<','<<info.maxNum<<']';}
    std::cout<<"],\"map_paths\":[";
    for(int area=0;area<18;++area){if(area)std::cout<<',';std::cout<<std::quoted(ss.mapFilename(area));}
    std::cout<<"],\"archives\":[";
    // Original disc consumers exercised in isolation; these diagnostic loads
    // do not open the menu or call unavailable Sscrn.rel widgets.
    std::map<std::string,ResourceLease> archives;
    bool archiveComma=false;
    for(const auto &f:files->entries()) {
        std::string path=f.path; std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(f.disc!=disc || !((path.rfind("ss/eng/",0)==0 && path.substr(path.size()-4)==".dat") || path.rfind("ss/cmn/map_obj",0)==0)) continue;
        auto h=game.loader().registerResource(f.path);
        execute([&]{game.loader().loadResource(h,Residency::Mram,true);});
        auto bytes=game.loader().resolve(game.loader().reference(h,0,f.size));
        check(hash(bytes)==f.sha256,"original SubScreen archive MRAM hash");
        archives.emplace(path,bytes);
        auto index=indexAssetBody(bytes.data,bytes.size);
        if(archiveComma)std::cout<<',';archiveComma=true;
        std::cout<<"{\"path\":"<<std::quoted(f.path)<<",\"sha256\":"<<std::quoted(hash(bytes))<<",\"entries\":[";
        for(std::size_t n=0;n<index.entries.size();++n){auto &e=index.entries[n];if(n)std::cout<<',';
            ResourceLease b{bytes.owner,bytes.data+e.offset,e.size}; std::string status="opaque-retained";
            if(!std::memcmp(e.tag.data(),"TPL",3)){try{(void)resolveMessageTpl(b,1024);status="checked-native-TPL";}catch(const std::invalid_argument &){status="TPL-open-unsupported-layout";}}
            std::string tag(e.tag.begin(),e.tag.end());while(!tag.empty() && !tag.back())tag.pop_back();
            std::cout<<"{\"index\":"<<e.index<<",\"tag\":"<<std::quoted(tag)<<",\"offset\":"<<e.offset<<",\"size\":"<<e.size<<",\"sha256\":"<<std::quoted(hash(b))<<",\"status\":"<<std::quoted(status)<<'}';
        }
        std::cout<<"]}"; game.loader().deleteResource(h);
    }
    std::cout<<"],\"model_consumers\":[";
    auto arc=[&](const std::string &path,u32 slot){auto bytes=archives.at(path);auto index=indexAssetBody(bytes.data,bytes.size);if(slot<4 || slot-4>=index.entries.size())throw std::out_of_range("original SS_ARC_PTR slot");auto &e=index.entries[slot-4];return ResourceLease{bytes.owner,bytes.data+e.offset,e.size};};
    game.engineConsumers().configureModelPools(0x80,0x100); // ss_map mapModelAlloc source capacities.
    bool modelComma=false;
    auto model=[&](const std::string &path,u32 binSlot,u32 tplSlot){
        auto bin=arc(path,binSlot),tpl=arc(path,tplSlot); u32 parts=0;
        for(int round=0;round<2;++round){auto h=game.engineConsumers().initializeModel(bin,tpl,path+std::to_string(binSlot));auto &object=game.engineConsumers().model(h);parts=object.nParts;
            check(parts==bin.data[0x19] && object.pModelInfo && object.Motion.Seq_speed==1,"original inventory/map modelInit");
            for(u32 n=0;n<parts;++n){auto parent=object.pModelInfo->model_addr->pHead[n].parentNo;auto *p=object.getPartsPtr(int(n));check(p->pParent==(parent==255?static_cast<ModelCoord*>(&object):object.getPartsPtr(parent)),"original inventory/map hierarchy");}
            game.engineConsumers().releaseModel(h);check(game.engineConsumers().modelCount()==0,"inventory/map model lifetime");}
        if(modelComma)std::cout<<',';modelComma=true;
        std::cout<<"{\"path\":"<<std::quoted(path)<<",\"bin_slot\":"<<binSlot<<",\"tpl_slot\":"<<tplSlot<<",\"parts\":"<<parts<<",\"rounds\":2}";
    };
    // Exact source references: pieceModelInit cases 0..3, doorModelInit and
    // MapGoal::init. This calls original modelInit, not those unported widgets.
    for(u32 n=0x1a6;n<=0x1a9;++n)model("ss/eng/ss_pzzl.dat",n,0x1a5);
    model("ss/eng/ss_map.dat",0xc,0xd);model("ss/eng/ss_map.dat",0xf,0x10);
    game.engineConsumers().clear();
    std::cout<<"],\"trace\":["; ssTrace.insert(ssTrace.end(),ss.trace().begin(),ss.trace().end());
    for(std::size_t i=0;i<ssTrace.size();++i){if(i)std::cout<<',';std::cout<<std::quoted(ssTrace[i]);}
    ss.clear(); check(ss.itemCapacity()==0 && !ss.snapshot().initialized,"final inventory/SubScreen release");
    check(hash(sr[1].bytes)==sr[1].identity.sha256,"retired SubScreen lease invalidated");
    std::cout<<"]},\"id_system\":{\"capacity\":"<<idBoot.capacity<<",\"alive_after_Cockpit\":"<<idBoot.alive<<",\"trace\":[";
    for(std::size_t i=0;i<idTrace.size();++i){if(i)std::cout<<',';std::cout<<std::quoted(idTrace[i]);}
    std::cout<<"]},\"save_state\":{\"rooms\":"<<saveBoot.rooms<<",\"saved_rooms\":"<<saveBoot.savedRooms<<",\"room_bytes\":"<<saveBoot.roomBytes<<",\"image_bytes\":"<<saveBoot.imageBytes<<",\"room_ids\":[";
    for(std::size_t i=0;i<saveBoot.roomIds.size();++i){if(i)std::cout<<',';std::cout<<saveBoot.roomIds[i];}
    std::cout<<"],\"sections\":[";for(std::size_t i=0;i<saveBoot.sectionOffsets.size();++i){if(i)std::cout<<',';std::cout<<saveBoot.sectionOffsets[i];}
    std::cout<<"],\"aggregate_save_load\":false,\"trace\":[";for(std::size_t i=0;i<saveTrace.size();++i){if(i)std::cout<<',';std::cout<<std::quoted(saveTrace[i]);}
    ids.clear(); saves.clear(); check(!ids.snapshot().initialized && !saves.snapshot().initialized,"ID/save final release");
    std::cout<<"]},\"native_layout\":{\"ID_UNIT\":"<<sizeof(ID_UNIT)<<",\"SAVE_DATA_HEAD\":"<<sizeof(SAVE_DATA_HEAD)<<"},\"blocker\":"<<std::quoted(blocker)<<",\"repeated_init_released\":true,\"checks\":"<<checks<<",\"trace\":[";
    for(std::size_t i=0;i<trace.size();++i){if(i)std::cout<<',';std::cout<<std::quoted(trace[i]);}
    std::cout<<"]}\n";
}
int main(int argc, char **argv) {
    try {
        if(argc>1 && std::string(argv[1])=="messages") { messages(argc,argv); return 0; }
        if (argc > 1 && std::string(argv[1]) == "data") {
            data(argc, argv);
            return 0;
        }
        if (argc > 1 && std::string(argv[1]) == "consumers") {
            consumers(argc, argv);
            return 0;
        }
        if (argc > 1 && std::string(argv[1]) == "models") {
            consumers(argc,argv,false);
            return 0;
        }
        if (argc != 3 || std::string(argv[1]) != "self-test")
            throw std::invalid_argument("self-test <fixture-directory>");
        std::cout << re4::port::verifyGameRuntime(argv[2]) << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
