#include "re4/port/game_runtime.h"
#include "re4/port/sha256.h"
#include "re4/port/motion.h"
#include "re4_player_types.h"
#include "../src/message_original.h"
#include "rnd.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <set>
#include <cmath>
#include <fstream>
namespace {
// Explicit CPU VI observer for CLI initialization tests; no GPU or display claim.
struct CpuPresentationProbe final:re4::port::GraphicsPresenter {
    re4::port::GraphicsClear last{};bool black{};u64 visits{};
    void present(const re4::port::GraphicsClear &c,bool b) override { last=c;black=b;++visits; }
};
std::string hash(re4::port::ResourceLease bytes) {
    return re4::port::sha256(bytes.data, bytes.size);
}
void weaponJson(const re4::port::LeonConstructionSnapshot &s){
    const auto &w=s.weapon;const auto &e=s.weaponEffect;
    std::cout<<"{\"read\":"<<(s.weaponRead?"true":"false")<<",\"initialized\":"<<(s.weaponInitialized?"true":"false")
        <<",\"module\":"<<w.module<<",\"mapped_weapon\":"<<w.mappedWeapon<<",\"path\":"<<std::quoted(w.path)<<",\"source_sha256\":"<<std::quoted(w.sourceHash)
        <<",\"body_sha256\":"<<std::quoted(w.bodyHash)<<",\"body_bytes\":"<<w.bodyBytes<<",\"sound_bytes\":"<<w.soundBytes<<",\"rel_offset\":"<<w.relOffset<<",\"rel_bytes\":"<<w.relBytes
        <<",\"object_id\":"<<s.weaponObjectId<<",\"item_id\":"<<s.weaponItemId<<",\"parts\":"<<s.weaponParts<<",\"parent_part\":"<<s.weaponParentPart
        <<",\"bin_sha256\":"<<std::quoted(s.weaponBinHash)<<",\"tpl_sha256\":"<<std::quoted(s.weaponTplHash)<<",\"motions\":[";
    for(std::size_t i=0;i<s.weaponBindings.size();++i){if(i)std::cout<<',';const auto &b=s.weaponBindings[i];std::cout<<"{\"index\":"<<b.index<<",\"slot\":"<<b.slot<<",\"path\":"<<std::quoted(b.path)<<",\"sha256\":"<<std::quoted(b.hash)<<'}';}
    std::cout<<"],\"effect\":{\"owner\":"<<e.owner<<",\"loads\":"<<e.loads<<",\"sha256\":"<<std::quoted(e.sourceHash)<<",\"est\":"<<e.estRecords<<",\"sst\":"<<e.sstRecords<<",\"paths\":"<<e.pathRecords<<",\"tex_objects\":"<<e.textureObjects<<",\"texture_ids\":[";
    for(std::size_t i=0;i<e.textureIds.size();++i){if(i)std::cout<<',';std::cout<<e.textureIds[i];}
    std::cout<<"],\"texture_frames\":[";for(std::size_t i=0;i<e.textureFrames.size();++i){if(i)std::cout<<',';std::cout<<e.textureFrames[i];}
    std::cout<<"],\"model_ids\":[";for(std::size_t i=0;i<e.modelIds.size();++i){if(i)std::cout<<',';std::cout<<e.modelIds[i];}
    std::cout<<"]}}";
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
    ResourceLease resetSat,resetEat;
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
        if(i==5) {
            // Hold real collision bytes for the explicit lifecycle test after
            // the original read/scenario measurements have been captured.
            auto block=[&](const char *tag) {
                auto it=std::find_if(room->index.entries.begin(),room->index.entries.end(),[&](const auto &e){return !std::memcmp(e.tag.data(),tag,3);});
                if(it==room->index.entries.end())throw std::runtime_error("real collision reset fixture missing");
                return ResourceLease{room->body,room->body->data()+it->offset,it->size};
            };
            resetSat=block("SAT");resetEat=block("EAT");
        }
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
    const auto readFrames=game.state().Frame_cnt;
    // Isolated reset contract on the one permitted original task domain. These
    // real resources are not a substitute gameRoomInit or a gameplay actor.
    game.sat().configure(1);game.eat().configure(1);
    auto satHandle=game.sat().create(resetSat),eatHandle=game.eat().create(resetEat);
    game.merchant().gameInit();game.initialize();
    if(game.merchant().snapshot().initialized)throw std::runtime_error("runtime reset retained merchant state");
    auto stale=[&](CollisionRuntime &owner,CollisionHandle old) {
        try {(void)owner.snapshot(old);}catch(const std::invalid_argument &){return;}
        throw std::runtime_error("runtime reset retained original collision reference");
    };
    stale(game.sat(),satHandle);stale(game.eat(),eatHandle);
    game.sat().configure(1);game.eat().configure(1);
    game.sat().create(resetSat);game.eat().create(resetEat);
    std::cout << "],\"scenario_blocker\":\"" << blocker
              << "\",\"frames\":" << readFrames << ",\"isolated_collision_reset\":true}\n";
}
} // namespace
// Actual shared boot consumers with private disc bytes, deliberately stops at
// the next missing original game dependency rather than inventing its state.
void messages(int argc,char **argv,bool newGame=false) {
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
    // Explicit startup input for the second boot case, not a fabricated result
    // or a Title_task claim. The original gameInit performs every state change.
    if(newGame)game.state().System_flg|=0x80000000u>>SYS_NEW_GAME;
    check(bool(game.startOriginalGameTask()),"original GameTask creation");
    game.graphics().attach(std::make_shared<CpuPresentationProbe>());
    std::string blocker;
    game.tick();
    check(game.state().Rno0==1 && game.state().Speed==1.f,"original gameInit completes to stage state");
    const auto stageDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    while(blocker.empty()) {
        try{game.tick();}catch(const MissingGameConsumer &failure){blocker=failure.what();}
        if(std::chrono::steady_clock::now()>stageDeadline)throw std::runtime_error("original gameStageInit timeout");
        std::this_thread::yield();
    }
    check(blocker=="Original consumer unavailable: gameRoomInit/model/light/player","actual GameTask completes original stage and reaches unbound room setup");
    check(game.state().Rno0==2 && game.state().SaveKind==-1,"stage's original state transition and save mode");
    const auto stageDisc=game.loader().disc();
    check(std::count(game.trace().begin(),game.trace().end(),"gameStageInit:original-complete")==1,"original stage body completed exactly once");
    auto &pg=game.state();auto stageImage=saves.image();
    check(stageImage.head->pGlobal->room_id==0x120 && stageImage.head->pGlobal->SaveKind==-1 && stageImage.head->pGlobal->pl_life==1200,"original aggregate global save");
    check(static_cast<MERCHANT_DATA *>(stageImage.head->pMr)->friendship==50,"aggregate calls actual merchant save");
    auto *stageInventory=static_cast<_ITEM_SAVE_DATA *>(stageImage.head->pItm);
    if(newGame) {
        const u16 originalIds[]={0x7c,0x23,4,5,0xac,0xad};
        for(std::size_t n=0;n<6;++n)check(stageInventory->item_list[n].id==originalIds[n],"actual original new-game inventory order");
        check(stageInventory->arm_no==1 && stageInventory->wep_id==0x23 && stageInventory->item_list[1].bullet==10 && stageInventory->item_list[2].num==20,"actual original start handgun, ammunition and armed slot");
        check(stageInventory->item_list[1].x==2 && stageInventory->item_list[1].y==1 && stageInventory->item_list[1].board==1,"original set_game handgun case coordinates");
        check(pg.point==5500 && pg.Game_level==5,"original GamePointInit -> GameAddPoint");
        check(std::all_of(std::begin(pg.item_save),std::end(pg.item_save),[](u8 v){return !v;}),"original SceAtInitSaveItem clears actual save block");
    } else {
        check(stageInventory->arm_no==0xffff && std::all_of(std::begin(stageInventory->item_list),std::end(stageInventory->item_list),[](const auto &v){return v.id==0xffff;}),"default boot saves original empty Title inventory; no synthetic start items");
    }
    const auto merchant=game.merchant().snapshot();
    check(merchant.initialized && !merchant.characterBound && merchant.data.friendship==50 &&
          !merchant.data.study_num && !merchant.data.reduction_ratio && !merchant.data.bonus_flag,"original MerchantGameInit reset");
    for(const auto &e:merchant.data.stock.e)check(e.id==0xffff && !e.num && !e.isNew,"original merchant stock reset");
    for(const auto &e:merchant.data.level.e)check(e.id==0xffff && std::all_of(std::begin(e.lv),std::end(e.lv),[](u8 v){return !v;}),"original tune reset");
    auto playerBoot=game.player().snapshot();auto playerTrace=game.player().trace();
    check(pg.pl_type==0 && pg.pl_life==1200 && pg.pl_life_max==1200 && pg.ashley_life==600 && pg.ashley_life_max==600,"original Leon life reset reached through GameTask");
    check(pg.weapon_no==2 && pg.weapon_type==0 && !(pg.weapon_lv_power || pg.weapon_lv_speed || pg.weapon_lv_blt || pg.weapon_lv_reload),"original Leon equipment reset");
    check(pg.pl_flag==1 && pg.weapon_no_old==0xff && playerBoot.kaiou==0 && playerBoot.oldWeaponId==0xff && playerBoot.weaponSlotEmpty,"original PlayerInit flags / weapon release");
    check(playerTrace==std::vector<std::string>{"PlayerInit:original","PlayerLifeReset:original","ReleaseWepData:original","InitModule:original"},"original PlayerInit dependency order");
    auto &lights=game.lights();auto lightBoot=lights.snapshot();auto lightPaths=lights.paths();
    check(lightBoot.initialized && lightBoot.pathsBound && lightBoot.capacity==0 && lightBoot.alive==0 && lightBoot.electricPower==1.0f,"original light boot state");
    check(std::all_of(lightBoot.kindFlags.begin(),lightBoot.kindFlags.end(),[](u32 f){return f==0xFFFFFFFFu;}),"original light kind reset");
    check(lights.trace()==std::vector<std::string>{"cLightMgr::init:original","cLightMgr::initPath:original"},"original light init order");
    check(lightPaths && lightPaths->offsets.size()==11 && lightPaths->offsets[7]==0,"actual CORE_LIGHT_PATH references");
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
    check(sr.size()==3 && ss.itemCapacity()==0x180 && bool(ss.findItem(0x7c))==newGame,"real Title inventory prerequisite / original new-game branch / three ARAM reads");
    u32 aram=0xD00000;
    for(auto &r:sr){check(r.aramAddress==aram && r.dmaBytes==((r.identity.size+31u)&~31u),"original rounded DMA offsets");check(hash(r.bytes)==r.identity.sha256,"actual ARAM to MRAM readback differs from FST hash"); aram+=r.dmaBytes;}
    check(sw.freeOffset==aram-0xD00000 && sw.commonOffset==sr[0].dmaBytes && sw.puzzleOffset==sr[0].dmaBytes+sr[1].dmaBytes,"original ARAM archive offsets");
    check(game.state().IsMessageInit==1 && game.state().Rno0==2,"original message / room-entry state");
    auto font=m.font(0),system=m.font(1);
    check(font.loaded && system.loaded && font.charWidth==32 && font.charHeight==32,"actual English font slots");
    check(font.textures.size()==system.textures.size() && font.widths.data==system.widths.data,"original English system-font alias");
    for(int n=0;n<16;++n) check(m.layout(n).queueBound==(n<3),"original glyph queue registration");
    std::cout<<"{\"pointer_bits\":"<<sizeof(void*)*8<<",\"kind\":\"actual_CoreDataRead_MessageControl_init_gameInit_Font_create\",\"light_boot\":{\"initialized\":true,\"paths\":"<<lightPaths->offsets.size()<<",\"path_sha256\":"<<std::quoted(hash(lightPaths->source))<<",\"trace\":[\"cLightMgr::init:original\",\"cLightMgr::initPath:original\"]},\"core_body_sha256\":"<<std::quoted(coreHash)<<",\"core_textures\":[";
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
    // Actual room/aggregate save bodies. Native images never masquerade as GC
    // card bytes. The live actor position branch remains an explicit error.
    auto nativeImage=saves.image();
    {
        auto originalGlobal=game.state();auto preservedSave=*nativeImage.head->pGlobal;
        game.state().pad_835C[0]=0x73;game.state().Em_flg[11][7]=0x12345678;
        check(saves.save(nativeImage,-1,game.state(),ss,game.merchant()),"actual original aggregate save");
        check(std::memcmp(nativeImage.head->pGlobal,static_cast<const GameSaveBlock *>(&game.state()),sizeof(GameSaveBlock))==0,"complete native global save tail, including currently unused fields");
        game.state().Rno0=3;auto kind=game.state().SaveKind;auto before=*nativeImage.head->pGlobal;
        rejects([&]{saves.save(nativeImage,7,game.state(),ss,game.merchant());});
        check(game.state().SaveKind==kind && std::memcmp(&before,nativeImage.head->pGlobal,sizeof(before))==0,"missing live actor position fails before save mutation");
        game.state()=originalGlobal;*nativeImage.head->pGlobal=preservedSave;
        saves.offsets(nativeImage);check(!saves.save(nativeImage,9,game.state(),ss,game.merchant()) && game.state().SaveKind==originalGlobal.SaveKind,"original offset-form save returns false without state change");saves.addresses(nativeImage);
    }
    for(auto room:saveBoot.roomIds){ check(!saves.passed(room,3),"original blank room record"); saves.setPassed(room,3); check(saves.passed(room,3)==0x10,"original room passed bit"); }
    saves.saveRooms(nativeImage);
    for(auto room:saveBoot.roomIds) saves.setPassed(room,5);
    saves.loadRooms(nativeImage);
    for(auto room:saveBoot.roomIds) check(saves.passed(room,3)==0x10 && !saves.passed(room,5),"original room state restored by ID");
    auto nativeCopy=saves.copyImage(nativeImage); check(nativeCopy.head->base==nativeCopy.head && nativeCopy.head!=nativeImage.head,"original checkAddr rebases copied native image");
    {
        // Isolated round-trip inputs after the actual GameTask stopped. Every
        // restore below executes the original body; no actor or frame is made.
        _ITEM_SAVE_DATA itemsBefore{};ss.saveInventory(itemsBefore);
        SSCRN_SAVE_DATA subBefore{};ss.saveState(subBefore);
        const auto merchantBefore=game.merchant().snapshot().data;
        auto oldState=game.state();
        check(saves.save(nativeCopy,-1,game.state(),ss,game.merchant()),"original aggregate save of current managers");
        const auto globalBefore=*nativeCopy.head->pGlobal;
        game.state().point=42;game.state().pad_835C[1]=0x57;game.state().Em_flg[9][5]=1;
        ss.clearInventory();SSCRN_SAVE_DATA alteredSub{};alteredSub.save=0x98765432;ss.loadState(alteredSub);
        auto alteredMerchant=merchantBefore;alteredMerchant.friendship=17;game.merchant().load(alteredMerchant);
        check(saves.load(nativeCopy,game.state(),ss,game.merchant()),"original aggregate normal load");
        check(std::memcmp(&globalBefore,static_cast<const GameSaveBlock *>(&game.state()),sizeof(GameSaveBlock))==0,"complete global value restored without changing native runtime fields");
        _ITEM_SAVE_DATA itemsAfter{};ss.saveInventory(itemsAfter);SSCRN_SAVE_DATA subAfter{};ss.saveState(subAfter);
        check(std::memcmp(&itemsBefore,&itemsAfter,sizeof(itemsBefore))==0 && subBefore.save==subAfter.save,"original inventory/SubScreen save-load roundtrip");
        auto merchantAfter=game.merchant().snapshot().data;
        check(std::memcmp(&merchantBefore,&merchantAfter,sizeof(merchantBefore))==0,"original merchant save-load roundtrip");
        game.loadGameState();check(game.state().RoomNo_next==game.state().room_id && game.state().Part_next==game.state().Part && game.state().pl_flag==1 && (game.state().System_flg&(0x80000000u>>SYS_LOAD_GAME)),"actual GameLoad destination and original load flags");
        game.state()=oldState;
        auto *badItems=static_cast<_ITEM_SAVE_DATA *>(nativeCopy.head->pItm);auto armed=badItems->arm_no;badItems->arm_no=0x180;
        rejects([&]{saves.load(nativeCopy,game.state(),ss,game.merchant());});badItems->arm_no=armed;
        check(game.state().point==oldState.point,"invalid inventory rejected before aggregate global mutation");
        auto mode=nativeCopy.head->pGlobal->SaveKind;nativeCopy.head->pGlobal->SaveKind=3;
        rejects([&]{saves.load(nativeCopy,game.state(),ss,game.merchant());});nativeCopy.head->pGlobal->SaveKind=mode;
        check(game.state().SaveKind==oldState.SaveKind,"unported new-round dependencies fail before restore");
    }
    saves.offsets(nativeCopy); check(!nativeCopy.head->base && reinterpret_cast<std::uintptr_t>(nativeCopy.head->pGlobal)==0x40,"original save offset form");
    check(!saves.load(nativeCopy,game.state(),ss,game.merchant()),"original offset-form load returns false");
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
    std::cout<<"],\"aggregate_save\":true,\"aggregate_load\":true,\"aggregate_save_load\":true,\"new_round_load\":false,\"trace\":[";for(std::size_t i=0;i<saveTrace.size();++i){if(i)std::cout<<',';std::cout<<std::quoted(saveTrace[i]);}
    ids.clear(); saves.clear(); check(!ids.snapshot().initialized && !saves.snapshot().initialized,"ID/save final release");
    std::cout<<"]},\"player\":{\"gameTask_PlayerInit_complete\":true,\"life\":1200,\"max_life\":1200,\"weapon\":2,\"weapon_type\":0,\"flag\":1,\"actor_constructed\":false,\"trace\":[";
    for(std::size_t i=0;i<playerTrace.size();++i){if(i)std::cout<<',';std::cout<<std::quoted(playerTrace[i]);}
    std::cout<<"],\"leon_assets\":[";
    auto request=game.loader().request("em/pl00.drs",0x8100);std::shared_ptr<const LoadedFile> playerFile;
    execute([&]{while(game.loader().readCheck(request,playerFile)==0)scheduler.sleep(1);});
    check(playerFile && playerFile->bodySize>0,"actual player DVD/Yz2 request");
    auto &pf=*playerFile;ResourceLease pl{playerFile,playerFile->bytes.data()+pf.bodyOffset,pf.bodySize};
    game.engineConsumers().configureModelPools(0x80,0x100);
    game.state().pl_type=0;game.state().pl_costume=0;
    // Shared consumer probe, not gameRoomInit: real CNS counts provide the
    // base object/effect capacities. Scroll objects are not registered here.
    check(game.loader().loadRoom(0x120,false),"original room load for lifecycle prerequisites");
    const auto poolDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    while(game.loader().roomBusy()){game.tick();std::this_thread::yield();if(std::chrono::steady_clock::now()>poolDeadline)throw std::runtime_error("room lifecycle prerequisite timeout");}
    auto poolRoom=game.loader().room();check(bool(poolRoom),"actual room/CNS prerequisite");
    check(poolRoom->body->size()<=UINT32_MAX,"native CNS lease extent");
    ResourceLease poolBody{poolRoom->body,poolRoom->body->data(),static_cast<u32>(poolRoom->body->size())};
    auto capacities=originalRoomConstants(poolBody);
    game.objects().configure(capacities[1]);game.effects().configurePools(capacities[2],capacities[3]);
    const auto playerIndex=indexAssetBody(pl.data,pl.size);
    auto playerSlot=[&](u32 slot){const auto &s=playerIndex.entries.at(slot-4);return ResourceLease{pl.owner,pl.data+s.offset,s.size};};
    // Actual player EFF includes empty TPL/ANM offsets. Exercise original
    // owner counting and release bodies without spawning any replacement FX.
    auto playerEffects=playerSlot(0x1a);const auto effectHash=hash(playerEffects);
    check(game.effects().loadData(playerEffects,3,0)==1,"original pl00 effect registration");
    auto originalEffects=game.effects().dataSnapshot(3);
    check(originalEffects.loads==1 && originalEffects.estRecords>0 && originalEffects.sourceHash==effectHash,"original pl00 effect table identity");
    check(game.effects().loadData(playerEffects,3,0)==1 && game.effects().dataSnapshot(3).loads==2,"original shared effect reference count");
    check(game.effects().loadData(playerEffects,3,1)==0 && game.effects().dataSnapshot(3).loads==3,"original duplicate-not-allowed still increments owner count");
    check(game.effects().releaseData(3,1,0)==1 && game.effects().dataSnapshot(3).loads==2,"original counted effect release retains owner");
    check(game.effects().releaseData(3,0,0)==1 && game.effects().dataSnapshot(3).loads==0,"original forced effect release clears all registrations");
    auto badEffects=std::make_shared<std::vector<u8>>(playerEffects.data,playerEffects.data+playerEffects.size);
    (*badEffects)[4]=255; // Out-of-resource first ID table, before publication.
    rejects([&]{game.effects().loadData({badEffects,badEffects->data(),static_cast<u32>(badEffects->size())},3,0);});
    check(game.effects().dataSnapshot(3).loads==0 && hash(playerEffects)==effectHash,"malformed EFF does not mutate original source or owner state");
    rejects([&]{game.effects().loadData({pl.owner,playerEffects.data,47},3,0);});
    // The shared model owner executes original modelInit with actual pl00 BIN
    // and TPL. This is an ownership probe, not a fake weapon model assignment.
    for(int round=0;round<2;++round){
        auto object=*game.objects().create(11);
        game.objects().initializeModel(object,playerSlot(4),playerSlot(5));
        check(game.objects().snapshot(object).model && game.engineConsumers().modelCount()==1,"shared object holds original pl00 model owner");
        execute([&]{game.objects().destroy(object,true);});
        check(game.engineConsumers().modelCount()==0,"original object push releases model inside scheduler");
        rejects([&]{game.objects().snapshot(object);});
    }
    // Actual preserved construction sequence in the existing player runtime,
    // using the imported pl00 owner. Expected failure is NOT actor success.
    ss.initializeInventory();
    execute([&]{ss.gameInit();});
    ss.inventoryRoomInit();
    struct WeaponCase{u8 no,type;LeonConstructionSnapshot snapshot;};std::vector<WeaponCase> weaponCases;
    f32 clothLengthError=0,clothNormalError=0;
    for(auto equipment:std::array<std::pair<u8,u8>,5>{{{2,0},{1,0},{3,0},{2,1},{2,0}}})for(int attempt=0;attempt<2;++attempt) {
        // game.initialize() above reset the player cache after the earlier
        // GameTask proof. Re-enter the original PlayerInit before each real
        // constructor attempt, rather than manufacturing a weapon/cache pair.
        game.player().gameInit();
        // Explicit isolated equipped-weapon inputs; not a new-game/save-state
        // mutation in GameTask. Selection/variants are the original functions.
        game.state().weapon_no=equipment.first;game.state().weapon_type=equipment.second;
        auto thrown=*game.objects().create(10);auto retained=*game.objects().create(0);
        auto live=game.heaps().liveBytes();std::string actorBlocker;
        execute([&] {
            check(bool(scheduler.current()),"actual constructor caller is a scheduler task");
            try {(void)game.player().constructLeon(pl);} catch(const MissingGameConsumer &e) {actorBlocker=e.what();}
        });
        auto result=game.player().constructionSnapshot();
        if(actorBlocker!="Original consumer unavailable: cPlayer::move/live virtual actor, DmgMgr, actionSelect, EmAtCheck and camera owners")throw std::runtime_error("actual Leon construction blocker: "+actorBlocker);
        check(result.clothInitialized && result.bodyInitialized && result.cameraAllocated && result.startUpEntered,"whole original init1 and actual startUp entry");
        check(result.routines==std::array<u8,4>{0,0,0,1},"original startUp routine transition precedes missing virtual move");
        check(result.position.x==game.state().pl_pos.x && result.position.y==game.state().pl_pos.y && result.position.z==game.state().pl_pos.z && result.rotation.y==game.state().pl_ang_y,
              "original startUp uses current native global player state, not a manufactured room start");
        for(u32 r=0;r<3;++r){const auto &m=result.lightInverse;auto &v=result.lightCenter;
            check(std::fabs(m[r*4]*v.x+m[r*4+1]*v.y+m[r*4+2]*v.z+m[r*4+3])<.002f,"original inverse light volume maps its actual joint centre to origin");}
        check(result.cloth[0].links.size()==24 && result.cloth[1].links.empty() && result.cloth[2].links.size()==21 && result.cloth[0].volumes==6 && result.cloth[2].volumes==4,"actual costume-zero original jacket/holster/hair work order");
        auto distance=[](const Vec &a,const Vec &b){auto x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return std::sqrt(x*x+y*y+z*z);};
        auto close=[](f32 a,f32 b){return std::fabs(a-b)<.002f;};
        for(const auto &chain:result.cloth)for(const auto &link:chain.links){
            check((link.motionFlags&0x06000000)==0x06000000 && !link.speed.x && !link.speed.y && !link.speed.z,"original pendulum setup enables joint flags and clears velocity");
            if(link.refs[2]!=255){
                clothLengthError=std::max(clothLengthError,std::fabs(link.length-distance(link.childWorld,link.jointWorld)));
                check(close(link.length,distance(link.childWorld,link.jointWorld)) && distance(link.position,link.childWorld)<.002f,"original pendulum length/position resolved against imported child joint");
                check(close(link.offset.x,link.childWorld.x-link.jointWorld.x) && close(link.offset.y,link.childWorld.y-link.jointWorld.y) && close(link.offset.z,link.childWorld.z-link.jointWorld.z),"original child-relative pendulum offset");
                if(link.length>0){
                    clothNormalError=std::max({clothNormalError,std::fabs(link.normal.x-link.offset.x/link.length),std::fabs(link.normal.y-link.offset.y/link.length),std::fabs(link.normal.z-link.offset.z/link.length)});
                    check(close(link.normal.x,link.offset.x/link.length) && close(link.normal.y,link.offset.y/link.length) && close(link.normal.z,link.offset.z/link.length),"original normalized direction compared with independent scalar reference");
                }
            }else if(link.refs[1]!=255){
                auto parent=std::find_if(chain.links.begin(),chain.links.end(),[&](const ClothLinkSnapshot &v){return v.refs[0]==link.refs[1];});
                check(parent!=chain.links.end() && link.length==parent->length && distance(link.offset,parent->offset)==0,"original chain tail inherits its initialized parent pendulum");
            }
            for(u32 n=0;n<4;++n)if(link.refs[n+3]!=255){
                auto neighbor=std::find_if(chain.links.begin(),chain.links.end(),[&](const ClothLinkSnapshot &v){return v.refs[0]==link.refs[n+3];});
                check(neighbor!=chain.links.end() && close(link.neighborLengths[n],distance(link.position,neighbor->position)*.5f),"original half-distance cloth neighbour constraint");
            }
        }
        for(const auto &shape:result.body){
            check(shape.maxFrame==257 && shape.tableCount==2 && shape.offsets==std::array<u32,2>{24,56} && shape.shapeIds==std::array<u16,2>{0,1} && shape.keyCounts==std::array<u16,2>{2,2},"original Body SPAE header, indices and offsets");
            check(shape.frames==std::array<s32,4>{0,256,0,256} && shape.values==std::array<f32,4>{1,0,0,1} && shape.left==std::array<f32,4>{} && shape.right==std::array<f32,4>{},"original complementary face-morph curves");
        }
        check(result.playerEffect.loads==1 && !result.playerEffect.sourceHash.empty() && !game.effects().dataSnapshot(result.playerEffect.owner).loads,"original pl00 effect load balanced on startUp failure");
        check(result.init1Reached && result.hitboxes==5 && result.lightFlags==1,"original Neck/light/atari/Yarare init1 prefix");
        check(result.weaponRead && result.weaponInitialized && result.weapon.loaded && result.weapon.initBound && result.weapon.moveBound && result.weapon.initialized,"actual ReadWepData, native prolog and original virtual weapon initialization");
        check(result.weaponEffect.loads==1 && !result.weaponEffect.sourceHash.empty() && result.weaponEffect.estRecords>0,"actual original weapon EspDataLoad registration");
        check(result.weaponParentPart==10 && result.weaponParts>0 && result.weaponMotions>0,"actual weapon virtual init/setMotion/player hand parent");
        check(!game.player().weapons().snapshot().loaded && !game.state().pWep && !game.effects().dataSnapshot(result.weaponEffect.owner).loads,"rollback unlinks native weapon and releases exactly its effect count");
        weaponCases.push_back({equipment.first,equipment.second,result});
        check(result.weaponReleased && game.objects().snapshot().active==1 && (game.objects().snapshot(thrown).flags&0x200),"original weaponRelease iterates live shared objects and defers thrown item destruction");
        check(game.objects().snapshot(retained).flags==0x21,"original weaponRelease retains unrelated object");
        check(!result.returnedActor && result.playerConstructor && result.subobjects && result.modelInitialized && result.rolledBack && result.schedulerTask,
              "preserved player/subwork/model construction ran; failed actor was not published");
        check(result.hp==game.state().pl_life && result.motionSlots==0x6D && result.registeredMotionSlots==12 && result.debugCallbacks==11,
              "original player life/table lengths/debug registrations");
        check(result.parts==119 && result.modelInfos==8,"constructor uses the original complete pl00 hierarchy and attachments");
        check(game.engineConsumers().modelCount()==0 && game.heaps().liveBytes()==live,"constructor failure releases shared model and work owners");
        rejects([&]{(void)game.player().leonModel();});
        game.objects().destroy(retained,true);game.objects().dieCheck();game.objects().dieCheck();
        check(game.objects().snapshot().occupied==0,"constructor cleanup drains original deferred objects");
    }
    const auto constructor=game.player().constructionSnapshot();
    ss.clear();
    game.effects().clear();game.objects().clear();
    for(int round=0;round<2;++round) {
        auto handle=game.player().initializeLeonAssets(pl);auto &cpu=game.engineConsumers().model(handle);auto infos=game.player().leonModelInfos();
        MotionCamera inactiveCamera;cpu.Motion.pAttachCam=&inactiveCamera;
        game.engineConsumers().cameraState.registAttachCamera(&inactiveCamera,&cpu);
        check(cpu.nParts==pl.data[indexAssetBody(pl.data,pl.size).entries[0].offset+0x19] && infos.size()==8 && cpu.TevScaleGroup==1,"original Leon model and eight material/model attachments");
        game.player().applyWaist(.5f);
        check(cpu.getPartsPtr(1)->inv_offset.y==.25f && cpu.getPartsPtr(2)->inv_offset.y==.25f && cpu.getPartsPtr(3)->inv_offset.y==-.5f,
              "original body distributes waist twist over actual Leon joints");
        game.player().applyWaist(0);
        auto *knife=game.player().knife(),*face=game.player().face();
        check(knife && knife->mat[0][0]==0 && knife->mat[1][1]==0 && knife->mat[2][2]==0,"original hidden knife matrix");
        check(face && !face->pShape && face->shape_frame==0,"original neutral face ShapeEnd");
        for(const auto &key:face->shape)check(key.rate==0 && !key.data,"original five neutral shape channels");
        for(u32 n=0;n<cpu.nParts;++n){auto parent=cpu.pModelInfo->model_addr->pHead[n].parentNo;auto *part=cpu.getPartsPtr(int(n));check(part->pParent==(parent==255?static_cast<ModelCoord*>(&cpu):cpu.getPartsPtr(parent)),"original Leon joint parent resolution");}
        auto bindings=game.player().motionBindings();check(bindings.size()==14,"original Leon motion registration count");
        if(round)std::cout<<',';std::cout<<"{\"archive_body_sha256\":"<<std::quoted(hash(pl))<<",\"parts\":"<<unsigned(cpu.nParts)<<",\"models\":"<<infos.size()<<",\"motions\":[";
        for(std::size_t i=0;i<bindings.size();++i){auto &b=bindings[i];if(i)std::cout<<',';std::cout<<"{\"motion_index\":"<<b.motionIndex<<",\"archive_slot\":"<<b.archiveSlot<<",\"sha256\":"<<std::quoted(hash(b.bytes))<<'}';}
        std::cout<<"],\"model_sources\":[";
        // Every info points to a resolved native BIN/TPL. Match them back to the
        // immutable original slots without reading native pointers as GC words.
        for(std::size_t i=0;i<infos.size();++i){if(i)std::cout<<',';auto *d=infos[i]->model_addr;auto *t=static_cast<TexturePaletteView*>(infos[i]->tpl_addr);
            check(d->resolved && t->resolved,"original Leon BIN/TPL relocation adapters");
            std::cout<<"{\"parts\":"<<unsigned(d->nParts)<<",\"textures\":"<<t->numDescriptors<<",\"bin_sha256\":"<<std::quoted(hash(d->source))<<",\"tpl_sha256\":"<<std::quoted(hash(t->bytes))<<'}';}
        std::cout<<"],\"sequence_initializations\":[";
        bool sequenceComma=false;
        for(const auto &motion:bindings) {
            if((motion.archiveSlot&1)!=0)continue;
            auto it=std::find_if(bindings.begin(),bindings.end(),[&](const auto &seq){return seq.motionIndex==motion.motionIndex+1;});
            check(it!=bindings.end(),"original Leon FCV/SEQ table pair");auto seq=resolveMotionSequence(it->bytes);
            game.engineConsumers().initializeMotion(handle,motion.bytes,it->bytes);
            check(inactiveCamera.type==0 && !game.engineConsumers().cameraState.getAttachCamera(&cpu) &&
                  std::all_of(std::begin(inactiveCamera.parts),std::end(inactiveCamera.parts),[](u8 p){return p==255;}),"original MotionSetCore unregisters allocated inactive camera and clears channel indices");
            const auto &w=cpu.Motion;
            check(w.pMot && w.pSeq_top && w.Seq_frame_num==seq.count && w.Seq.Frame==seq.keys[0].Frame && w.Seq.Se==seq.keys[0].Se && w.Seq.Free==seq.keys[0].Free,"actual original MotionSetCore initial sequence state");
            if(sequenceComma)std::cout<<',';sequenceComma=true;
            std::cout<<"{\"motion_index\":"<<motion.motionIndex<<",\"max_frame\":"<<w.Mot_frame_max<<",\"joints\":"<<unsigned(w.Joint_num)<<",\"sequence_count\":"<<seq.count<<",\"sequence_flags\":"<<unsigned(seq.flags)<<",\"first_key\":["<<w.Seq.Frame<<','<<unsigned(w.Seq.Se)<<','<<unsigned(w.Seq.Free)<<"],\"mot_attr\":"<<w.Mot_attr<<",\"sequence_keys\":[";
            for(std::size_t n=0;n<seq.keys.size();++n){if(n)std::cout<<',';auto &k=seq.keys[n];std::cout<<'['<<k.Frame<<','<<unsigned(k.Se)<<','<<unsigned(k.Free)<<']';}
            std::cout<<"],\"ticks\":[";
            // Isolated motion owner, not a constructed Leon actor: explicit actor
            // status view with an empty original SAT manager (no floor hit).
            MotionActorContext context{&game.sat(),false,0x100};
            for(u32 tick=0;tick<16;++tick) {
                cpu.matCalc(); // Original model-coordinate update by this isolated owner.
                auto result=game.engineConsumers().advanceMotion(handle,1.f,&context);
                auto frame=std::min<u32>(tick+1,seq.count-1);auto key=seq.keys[frame];
                check(w.Seq_frame==f32(frame) && w.Seq.Frame==key.Frame && w.Seq.Se==key.Se && w.Seq.Free==key.Free,"original MotionMove/SEQ persistent tick");
                check(result==w.Mot_state,"original MotionMove result/state");
                for(u32 n=0;n<cpu.nParts;++n){auto *p=cpu.getPartsPtr(int(n));check(std::isfinite(p->pos.x+p->pos.y+p->pos.z+p->ang.x+p->ang.y+p->ang.z),"original evolving part pose finite");}
                for(u32 n=0;n<cpu.nParts;++n){auto *p=cpu.getPartsPtr(int(n));for(const auto &row:p->mat)for(auto value:row)check(std::isfinite(value),"original IK world matrix finite");}
                if(w.Mot_frame>w.Mot_frame_old && !(w.Mot_flag&0x04000000))check(motionCrossedFrame(cpu,w.Mot_frame)==1,"original animation crossing event");
                if(tick)std::cout<<',';
                std::cout<<"{\"sequence_frame\":"<<w.Seq_frame<<",\"motion_frame\":"<<w.Mot_frame<<",\"key\":["<<w.Seq.Frame<<','<<unsigned(w.Seq.Se)<<','<<unsigned(w.Seq.Free)<<"],\"state\":"<<result<<'}';
            }
            std::cout<<"],\"ik_blocked\":false}";
            // Original SEQ progression semantics on the same disc pair: loop,
            // reverse, pause and fractional time. These are isolated input modes,
            // not invented gameplay state transitions.
            game.engineConsumers().initializeMotion(handle,motion.bytes,it->bytes,0,5);
            for(u32 n=0;n<u32(seq.count)+3;++n){cpu.matCalc();game.engineConsumers().advanceMotion(handle,1.f,&context);auto expected=(n+1)%seq.count;check(w.Seq_frame==expected && w.Seq.Frame==seq.keys[expected].Frame,"original loop and wrap");}
            if(seq.count>2){game.engineConsumers().initializeMotion(handle,motion.bytes,it->bytes,int(seq.count-1),3);for(u32 n=0;n<2;++n){cpu.matCalc();game.engineConsumers().advanceMotion(handle,1.f,&context);check(w.Seq_frame==f32(seq.count-2-n),"original reverse sequence progression");}}
            game.engineConsumers().initializeMotion(handle,motion.bytes,it->bytes,0,9);game.engineConsumers().advanceMotion(handle,1.f,&context);
            check(w.Seq_frame==0 && w.Seq.Se==0 && w.Seq.Frame==seq.keys[0].Frame,"original paused SEQ suppresses SE key");
            if(seq.count>1){game.engineConsumers().initializeMotion(handle,motion.bytes,it->bytes);game.engineConsumers().advanceMotion(handle,.5f,&context);auto expected=u16(seq.keys[0].Frame+u16(.5f*(int(seq.keys[1].Frame)-int(seq.keys[0].Frame))));check(w.Seq_frame==.5f && w.Seq.Frame==expected,"original fractional SEQ interpolation");}
            game.engineConsumers().initializeMotion(handle,motion.bytes,it->bytes,0,1,2);
            for(u8 remaining:{u8(1),u8(0)}) {
                cpu.matCalc();game.engineConsumers().advanceMotion(handle,1.f,&context);
                check(w.Hokan_cnt==remaining,"actual original Hokan completes on owned Leon FCV/SEQ");
                for(auto *part=cpu.pList;part;part=part->pList)
                    for(const auto &row:part->mat)for(auto value:row)check(std::isfinite(value),"Hokan Leon matrix finite");
            }
            for(int frame:{int(seq.count/2),int(seq.count-1)}){game.engineConsumers().initializeMotion(handle,motion.bytes,it->bytes,frame);check(cpu.Motion.Seq.Frame==seq.keys[frame].Frame && cpu.Motion.Seq.Se==seq.keys[frame].Se && cpu.Motion.Seq.Free==seq.keys[frame].Free,"original sequence start at middle/last frame");}
            // Real pl00 FCV/SEQ and original cMotBase bodies; isolated owner,
            // not cPlayer::move or a gameplay movement decision.
            game.engineConsumers().initializeMotion(handle,motion.bytes,it->bytes);
            PlayerMotionBaseCpu base;Vec start=cpu.pos,angle=cpu.ang;base.set(&cpu,&start,&angle,4);
            check(base.modelOwner()==&cpu && base.counter()==4 && !(cpu.Motion.Mot_attr&1),"original cMotBase binds the actual native motion owner");
            for(u8 count:{u8(3),u8(2),u8(1),u8(255)}){
                base.adjust();cpu.matCalc();game.engineConsumers().advanceMotion(handle,1.f,&context);base.move();
                check(base.counter()==count && std::isfinite(cpu.pos.x+cpu.pos.y+cpu.pos.z+cpu.ang.y),"original cMotBase root-motion blend progresses over imported FCV ticks");
            }
            check((cpu.Motion.Mot_attr&1)!=0,"original cMotBase returns root-motion ownership after blend");
            inactiveCamera.type=1;const auto savedFrame=cpu.Motion.Seq_frame;
            rejects([&]{game.engineConsumers().advanceMotion(handle,1.f,&context);});
            check(cpu.Motion.Seq_frame==savedFrame,"active camera presentation remains explicit failure before motion mutation");inactiveCamera.type=0;
            rejects([&]{resolveMotionSequence({it->bytes.owner,it->bytes.data,3});});
        }
        std::cout<<"]}";
        game.player().setHands(0,0);game.player().setHands(0,0x63);
        check(game.player().leonModelInfos().size()==8,"original hand replacement deletes old infos");
        execute([&]{rejects([&]{game.player().releaseLeonAssets();});});
        game.engineConsumers().cameraState.registAttachCamera(&inactiveCamera,&cpu);
        auto retainedHash=hash(bindings[0].bytes);game.player().releaseLeonAssets();check(game.engineConsumers().modelCount()==0,"original Leon joint/model release");
        check(!game.engineConsumers().cameraState.getAttachModel(nullptr) && !game.engineConsumers().cameraState.m_attach_num,"model retirement leaves no attach-camera references");
        rejects([&]{(void)game.engineConsumers().model(handle);});check(hash(bindings[0].bytes)==retainedHash,"retired motion reference remains leased");
    }
    auto finalHandle=game.player().initializeLeonAssets(pl);game.initialize();
    rejects([&]{(void)game.engineConsumers().model(finalHandle);});
    check(game.engineConsumers().modelCount()==0 && game.player().motionBindings().empty(),"GameRuntime reset releases original Leon assets");
    rejects([&]{game.player().initializeLeonAssets({pl.owner,pl.data,15});});
    check(game.engineConsumers().modelCount()==0,"malformed player archive did not publish model");
    std::cout<<"],\"weapon_cases\":[";
    for(std::size_t i=0;i<weaponCases.size();++i){if(i)std::cout<<',';auto &c=weaponCases[i];std::cout<<"{\"equipped_weapon\":"<<unsigned(c.no)<<",\"type\":"<<unsigned(c.type)<<",\"scope\":\"isolated-original-constructor\",\"weapon\":";weaponJson(c.snapshot);std::cout<<'}';}
    std::cout<<"],\"cloth_body\":{\"cloth_initialized\":"<<(constructor.clothInitialized?"true":"false")<<",\"body_initialized\":"<<(constructor.bodyInitialized?"true":"false")<<",\"camera_allocated\":"<<(constructor.cameraAllocated?"true":"false")<<",\"startUp_entered\":"<<(constructor.startUpEntered?"true":"false")<<",\"links\":["<<constructor.cloth[0].links.size()<<","<<constructor.cloth[1].links.size()<<","<<constructor.cloth[2].links.size()<<"],\"volumes\":["<<constructor.cloth[0].volumes<<","<<constructor.cloth[1].volumes<<","<<constructor.cloth[2].volumes<<"],\"length_max_abs_error\":"<<std::setprecision(9)<<clothLengthError<<",\"normal_max_abs_error\":"<<clothNormalError<<std::setprecision(6)<<",\"player_effect_sha256\":"<<std::quoted(constructor.playerEffect.sourceHash)<<",\"simulation_executed\":false},\"construction\":{\"returned_actor\":false,\"em_mgr_dispatch_bound\":false,\"player_constructor\":true,\"subobjects\":true,\"scheduler_task\":true,\"weapon_release_complete\":true,\"startUp_executed\":false,\"rolled_back\":true,\"parts\":"<<constructor.parts<<",\"model_infos\":"<<constructor.modelInfos<<",\"motion_slots\":"<<constructor.motionSlots<<",\"registered_motion_slots\":"<<constructor.registeredMotionSlots<<",\"debug_callbacks\":"<<constructor.debugCallbacks<<",\"init1_reached\":"<<(constructor.init1Reached?"true":"false")<<",\"hitboxes\":"<<constructor.hitboxes<<",\"light_flags\":"<<constructor.lightFlags<<",\"blocker\":"<<std::quoted(constructor.blocker)<<"}},\"merchant_boot\":{\"original_complete\":true,\"friendship\":"<<int(merchant.data.friendship)<<",\"character_bound\":false},\"game_init\":{\"original_complete\":true,\"next_state\":1,\"presentation\":\"isolated_CPU_recording_probe\",\"new_game_inventory_branch\":"<<(newGame?"true":"false")<<"},\"game_stage_init\":{\"original_complete\":true,\"next_state\":2,\"save_kind\":-1,\"disc_after\":"<<int(stageDisc)<<"},\"native_layout\":{\"ID_UNIT\":"<<sizeof(ID_UNIT)<<",\"SAVE_DATA_HEAD\":"<<sizeof(SAVE_DATA_HEAD)<<"},\"blocker\":"<<std::quoted(blocker)<<",\"repeated_init_released\":true,\"checks\":"<<checks<<",\"trace\":[";
    for(std::size_t i=0;i<trace.size();++i){if(i)std::cout<<',';std::cout<<std::quoted(trace[i]);}
    std::cout<<"]}\n";
}
// Isolated execution of real original light consumers on private loaded archives.
// It does not advance gameRoomInit, create model owners, or submit GPU lighting.
void lights(int argc,char **argv) {
    using namespace re4::port;
    if(argc<6) throw std::invalid_argument("lights <root> <catalog> <disc> <room-hex>...");
    auto disc=std::stoul(argv[4]);if(disc>1)throw std::invalid_argument("disc");
    auto files=std::make_shared<ImportedFiles>(argv[2],argv[3]);ControllerInput input;
    GameRuntime game(files,input,u8(disc));game.initialize();auto &l=game.lights();auto &scheduler=game.loader().scheduler();
    int checks=0;auto check=[&](bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);};
    auto rejects=[&](const TaskEntry &call){bool rejected=false;try{call();}catch(const std::exception &){rejected=true;}check(rejected,"invalid light input accepted");};
    auto load=[&](const char *path) {
        auto request=game.loader().request(path,0x8001);std::shared_ptr<const LoadedFile> file;
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
        while(game.loader().status(request)==LoadStatus::Pending || game.loader().status(request)==LoadStatus::Reading) { game.tick();std::this_thread::yield();if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("light original read deadline"); }
        check(game.loader().readCheck(request,file)==1 && bool(file),"light original async archive read");
        return ResourceLease{file,file->bytes.data()+file->bodyOffset,file->bodySize};
    };
    auto core=load("etc/core.das");auto ci=indexAssetBody(core.data,core.size);
    const auto &pe=ci.entries.at(ORIGINAL_CORE_LIGHT_PATH_SLOT-4);
    ResourceLease paths{core.owner,core.data+pe.offset,pe.size};
    RndInit(0x1234); // Isolated deterministic RNG seed, not a gameplay override.
    l.initialize();l.initializePaths(paths);auto pathView=l.paths();
    std::cout<<"{\"pointer_bits\":"<<sizeof(void*)*8<<",\"kind\":\"isolated_actual_original_light_consumers\",\"paths\":[";
    bool comma=false;
    for(u32 id=0;id<pathView->offsets.size();++id) {
        if(!pathView->offsets[id]){rejects([&]{(void)l.pathSteps(u8(id),0,1);});continue;}
        if(comma)std::cout<<',';comma=true;
        std::cout<<"{\"id\":"<<id<<",\"offset\":"<<pathView->offsets[id]<<",\"length\":"<<pathView->lengths[id]<<",\"sequences\":[";
        for(u8 flags=0;flags<4;++flags) {
            auto sequence=l.pathSteps(u8(id),flags,pathView->lengths[id]+3);check(sequence.size()==pathView->lengths[id]+3,"original path steps");
            if(flags)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<sequence.size();++j){if(j)std::cout<<',';std::cout<<sequence[j];}std::cout<<']';
        }
        std::cout<<"]}";
    }
    l.clear();
    std::cout<<"],\"archives\":[";
    for(int a=5;a<argc;++a) {
        auto roomNo=std::stoul(argv[a],nullptr,16);if(roomNo>UINT16_MAX)throw std::invalid_argument("room ID");
        check(game.loader().loadRoom(u16(roomNo)),"original ReadAreaData room start");
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
        while(game.loader().roomBusy()){game.tick();std::this_thread::yield();if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("light room decode deadline");}
        auto archive=game.loader().room();check(bool(archive),"original ReadAreaData decoded room");
        ResourceLease room{archive->body,archive->body->data(),u32(archive->body->size())};auto sourceHash=hash(room);auto ri=archive->index;
        std::optional<LightHandle> previous;
        if(a!=5)std::cout<<',';
        std::cout<<"{\"path\":"<<std::quoted(archive->source->identity.path)<<",\"body_sha256\":"<<std::quoted(sourceHash)<<",\"rounds\":[";
        for(int round=0;round<2;++round) {
            l.clear();l.initialize();l.initializePaths(paths);l.bindArchives(core,room);
            if(previous) rejects([&]{(void)l.state(*previous);});
            auto s=l.snapshot();check(s.alive==0 && s.capacity>0 && s.pathsBound && s.electricPower==1.0f,"original room light reset");
            check(l.roomSelected(),"original room block selection");l.selectBlock(0);check(!l.roomSelected(),"original event/core lighting selection");l.selectBlock(-1);check(l.roomSelected(),"original room lighting restoration");
            l.event(true);check(!l.kind(0x7F),"original beginEvent light kind");l.event(false);check(l.kind(0x7F),"original endEvent light kind");
            l.kind(255,false);check(!l.kind(255),"unsigned high kind bit");l.kind(255,true);check(l.kind(255),"kind restore");
            check(l.power(-2.0f)==0.0f && l.power(2.0f)==1.0f,"original electric power clamping");
            l.fogDistances(123.0f,456.0f);check(l.snapshot().environment.Fog.Start==123.0f && l.snapshot().environment.Fog.End==456.0f,"original event fog scalar changes");
            rejects([&]{(void)l.cut(1,-1);});rejects([&]{(void)l.cut(1,65535);});
            if(round)std::cout<<',';
            std::cout<<"{\"capacity\":"<<s.capacity<<",\"blocks\":[";
            for(int b=0;b<3;++b) {
                auto block=l.block(b);auto *header=block->header();u32 cuts=0,records=0,created=0,blocked=0;
                std::array<u32,17> handlerCalls{};
                std::vector<u8> scalarBytes;
                std::vector<u8> stateBytes;
                auto scalar=[&](u32 v){scalarBytes.push_back(u8(v>>24));scalarBytes.push_back(u8(v>>16));scalarBytes.push_back(u8(v>>8));scalarBytes.push_back(u8(v));};
                auto real=[&](f32 f){u32 bits{};std::memcpy(&bits,&f,4);scalar(bits);};
                // Compare actual dynamic integer/color states across ABIs and
                // repetitions, separately from the independent wire-scalar proof.
                auto stateTrace=[&](const LightStateView &s) {
                    for(auto v:{s.flags,s.parentNo,u32(s.routine)}) {
                        stateBytes.push_back(u8(v>>24));stateBytes.push_back(u8(v>>16));
                        stateBytes.push_back(u8(v>>8));stateBytes.push_back(u8(v));
                    }
                    for(auto v:{s.displayColor.r,s.displayColor.g,s.displayColor.b,s.displayColor.a})stateBytes.push_back(v);
                };
                for(u32 n=0;n<header->CutNum;++n) {
                    if(!block->cutOffsets[n])continue;++cuts;
                    auto env=l.cut(b,int(n));auto all=l.records(b,int(n));check(all.size()==env.nLight,"original LIT getCut/getLightWork count");records+=u32(all.size());
                    l.tune(b,int(n));auto tuned=l.snapshot().tune;
                    check((env.tuneOn&1)?std::memcmp(tuned.data(),env.Tune,12)==0:(tuned[0].r==0xC8 && tuned[1].r==0 && tuned[2].a==0x80),"original tune state");
                    for(u32 w=0;w<all.size();++w) {
                        const auto &original=all[w];
                        scalar(n);scalar(w);real(original.Pos.x);real(original.Pos.y);real(original.Pos.z);
                        real(original.Radius);real(original.Intensity);scalar(original.ParentNo);scalar(original.HitRadius);
                        real(original.spot.Normal.x);real(original.spot.Normal.y);real(original.spot.Normal.z);
                        try {
                            auto h=l.create(b,int(n),int(w),(w&1)!=0);check(bool(h),"original light record allocation");
                            if(!h)continue;++created;auto state=l.state(*h);
                            check(state.type==original.Type && state.id==original.Id && state.kind==original.Kind && state.enableMask==original.EnableMask && state.flags==original.BeFlag && state.index==0xFFFF,"original light assignment fields");
                            check(state.parentNo==(original.ParentType>4?1:original.ParentNo) && state.parentType==(original.ParentType>4?0:original.ParentType),"original parent scalar split/default handling");
                            check(std::memcmp(&state.color,&original.Col,4)==0,"original light base color");
                            check(std::memcmp(&state.position,&original.Pos,sizeof(Vec))==0 && state.radius==original.Radius && state.intensity==original.Intensity,"original light scalar assignment");
                            stateTrace(state);
                            if(original.Id==7) {
                                auto &v=state.direction;auto square=v.x*v.x+v.y*v.y+v.z*v.z;
                                check(std::fabs(square-100000.0f*100000.0f)<3000.0f,"original directional handler geometric invariant");
                            }
                            if(original.Id==0 || original.Id>=9) {
                                if(original.Id!=16)check(std::memcmp(&state.displayColor,&original.Col,4)==0,"original static color handler");
                            }
                            l.moveType(*h);++handlerCalls[original.Id];
                            stateTrace(l.state(*h));
                            l.destroy(*h);rejects([&]{(void)l.state(*h);});previous=h;
                            check(l.snapshot().alive==0,"original light destroy/unlink");
                        } catch(const MissingGameConsumer &) { ++blocked;check(l.snapshot().alive==0,"missing parent manager left published light"); }
                    }
                }
                if(b)std::cout<<',';
                std::cout<<"{\"sha256\":"<<std::quoted(hash(block->source))<<",\"scalar_sha256\":"<<std::quoted(sha256(scalarBytes.data(),scalarBytes.size()))<<",\"dynamic_state_sha256\":"<<std::quoted(sha256(stateBytes.data(),stateBytes.size()))<<",\"source_version\":"<<unsigned(block->sourceVersion)<<",\"native_version\":"<<unsigned(header->Version)<<",\"cut_count\":"<<header->CutNum<<",\"present_cuts\":"<<cuts<<",\"records\":"<<records<<",\"created\":"<<created<<",\"blocked_model_owners\":"<<blocked<<",\"handler_calls\":[";
                for(std::size_t n=0;n<handlerCalls.size();++n){if(n)std::cout<<',';std::cout<<handlerCalls[n];}std::cout<<"]}";
            }
            std::cout<<"]}";
            auto holding=scheduler.exec(1,[&]{scheduler.sleep(1);scheduler.exit();});check(bool(holding),"light lifetime task");
            game.tick();check(scheduler.allocatedStacks()!=0,"light lifetime task actually suspended");
            rejects([&]{l.clear();});scheduler.kill(*holding);
            auto leased=l.block(1);l.clear();check(hash(leased->source)==hash({room.owner,room.data+ri.find("LIT",0)->offset,ri.find("LIT",0)->size}),"retired light resource lease");
            check(!l.snapshot().initialized && l.snapshot().capacity==0,"light pool final release");
            if(previous)rejects([&]{(void)l.state(*previous);});
        }
        check(hash(room)==sourceHash,"LIT versionUp mutated original immutable archive");
        auto *entry=ri.find("LIT",0);auto mutated=std::make_shared<std::vector<u8>>(room.data+entry->offset,room.data+entry->offset+entry->size);
        ResourceLease bad{mutated,mutated->data(),u32(mutated->size())};(*mutated)[4]=0xFF;(*mutated)[5]=0xFF;(*mutated)[6]=0xFF;(*mutated)[7]=0xFC;
        rejects([&]{(void)resolveLightBlock(bad);});bad.size=3;rejects([&]{(void)resolveLightBlock(bad);});
        auto malformed=std::make_shared<std::vector<u8>>(paths.data,paths.data+paths.size);
        ResourceLease badPaths{malformed,malformed->data(),u32(malformed->size())};std::fill(malformed->begin()+4,malformed->begin()+8,0xFF);
        rejects([&]{(void)resolveLightPaths(badPaths);});
        std::cout<<"]}";
    }
    std::cout<<"],\"checks\":"<<checks<<",\"gpu_lighting\":false,\"full_lightMove\":false}\n";
}
// Real original SAT/EAT algorithms, isolated ray inputs chosen by the independent
// test driver. No actor placement or camera is manufactured here.
void collisionQueries(int argc,char **argv) {
    using namespace re4::port;
    if(argc!=5)throw std::invalid_argument("collision <SAT-or-EAT-file> <query-file> <effect-0-or-1>");
    auto bytes=std::make_shared<std::vector<u8>>();std::ifstream stream(argv[2],std::ios::binary);
    if(!stream)throw std::runtime_error("collision file missing");bytes->assign(std::istreambuf_iterator<char>(stream),{});
    NativeGameState state{};CollisionRuntime runtime(state,std::stoi(argv[4])!=0);runtime.configure(1);
    ResourceLease lease{bytes,bytes->data(),u32(bytes->size())};auto originalHash=hash(lease);auto handle=runtime.create(lease);auto info=runtime.snapshot(handle);
    std::ifstream queries(argv[3]);if(!queries)throw std::runtime_error("collision queries missing");
    std::cout<<std::setprecision(9)<<"{\"pointer_bits\":"<<sizeof(void*)*8<<",\"kind\":\"isolated_original_SAT_EAT_ray_floor\",\"sha256\":"<<std::quoted(hash(lease))<<",\"counts\":["<<info.vertices<<','<<info.normals<<','<<info.edges<<','<<info.polygons<<','<<info.blocks<<"],\"queries\":[";
    Vec from{},to{};int flags{},mask{};bool comma=false;
    std::vector<Vec> actorChecks;
    while(queries>>from.x>>from.y>>from.z>>to.x>>to.y>>to.z>>flags>>mask) {
        auto hit=runtime.hitCheck(from,to,flags,mask);
        auto movedFrom=from,movedTo=to;
        for(auto *p:{&movedFrom,&movedTo}){p->x+=17;p->y-=31;p->z+=53;}
        runtime.setCoord(handle,{17,-31,53},{});auto moved=runtime.hitCheck(movedFrom,movedTo,flags,mask);runtime.setCoord(handle,{},{});
        if(moved.attribute!=hit.attribute || std::abs(moved.point.x-hit.point.x-17)>.05f || std::abs(moved.point.y-hit.point.y+31)>.05f || std::abs(moved.point.z-hit.point.z-53)>.05f)throw std::runtime_error("original SAT setCoord ray transformation");
        if(from.x==to.x && from.z==to.z && from.y>=to.y) {
            auto floor=runtime.getFloor(from,0,from.y-to.y,mask);
            auto floorBottom=from;floorBottom.y=from.y-(from.y-to.y);
            auto floorHit=runtime.hitCheck(from,floorBottom,0x40,mask);
            if(floor!=(floorHit.attribute?floorHit.point.y:-100000.f))throw std::runtime_error("original SAT getFloor ray chain");
            if(floorHit.attribute && actorChecks.size()<3) {
                // Probe positions originate in the existing SAT triangle-ray
                // inventory. This runs original actor scenery collision, not
                // cPlayer movement, gameplay input or a room start pose.
                auto actor=std::make_shared<ActorModelCpu>();actor->pos=actor->pos_old=from;
                actor->atari.init(0,0,0,1,1,1,2,0,0x1000,0);
                for(int tick=0;tick<3;++tick){actor->atari.move();runtime.checkActor(actor,mask);actor->pos_old=actor->pos;}
                auto finalFloor=runtime.getFloor(actor->pos,600,100000,mask);
                if(finalFloor!=-100000.f && std::fabs(finalFloor-actor->pos.y)<1000 && std::fabs(finalFloor-actor->pos.y)>.05f)
                    throw std::runtime_error("original actor floor snap with real SAT data");
                if(actor->pFloor_norm && actor->pFloor_norm!=&actor->nativeFloorNormal)
                    throw std::runtime_error("actor floor normal owner");
                actorChecks.push_back(actor->pos);
            }
        }
        if(comma)std::cout<<',';comma=true;
        std::cout<<"{\"attribute\":"<<hit.attribute<<",\"point\":["<<hit.point.x<<','<<hit.point.y<<','<<hit.point.z<<"],\"normal\":["<<hit.normal.x<<','<<hit.normal.y<<','<<hit.normal.z<<"]}";
    }
    if(!queries.eof())throw std::invalid_argument("collision query syntax");
    auto rejects=[&](const auto &f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("collision invalid input accepted");};
    rejects([&]{runtime.create(lease);});rejects([&]{runtime.configure(2);});
    runtime.enable(handle,false);if(runtime.hitCheck(from,to).attribute)throw std::runtime_error("disabled SAT participated");
    runtime.enable(handle,true);runtime.setCoord(handle,{0,0,0},{0,0,0});runtime.destroy(handle);
    rejects([&]{runtime.snapshot(handle);});auto second=runtime.create(lease);runtime.clear();rejects([&]{runtime.enable(second,true);});runtime.configure(1);
    for(u32 size:{0u,1u,19u})rejects([&]{runtime.create({lease.owner,lease.data,size});});
    if(hash(lease)!=originalHash)throw std::runtime_error("SAT mutation");
    std::cout<<"],\"actor_checks\":[";
    for(std::size_t n=0;n<actorChecks.size();++n){if(n)std::cout<<',';auto p=actorChecks[n];std::cout<<'['<<p.x<<','<<p.y<<','<<p.z<<']';}
    std::cout<<"],\"actor_ticks_per_probe\":3,\"player_ticks\":false,\"released_and_repeated\":true}\n";
}
int main(int argc, char **argv) {
    try {
        if(argc>1 && std::string(argv[1])=="collision") {collisionQueries(argc,argv);return 0;}
        if(argc>1 && std::string(argv[1])=="lights") { lights(argc,argv);return 0; }
        if(argc>1 && std::string(argv[1])=="messages") { messages(argc,argv); return 0; }
        if(argc>1 && std::string(argv[1])=="new-game") { messages(argc,argv,true); return 0; }
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
