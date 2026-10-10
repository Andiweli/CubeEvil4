#include "re4/port/game_runtime.h"
#include "re4/port/sha256.h"
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
        auto event = engine.event(raw);
        auto &package = engine.event(event).package;
        auto refs=indexEventResourceReferences(package);
        auto findBin=[&](const std::string &name)->const EventBinView* {
            auto it=std::find_if(package.bins.begin(),package.bins.end(),[&](const auto &b){return b.name==name;});
            return it==package.bins.end()?nullptr:&*it;
        };
        u32 diagnosticParts=2;
        for(const auto &ref:refs) if(ref.type!=11) {
            auto *bin=findBin(ref.bin);
            if(bin && bin->bytes.size>=0x40) diagnosticParts=std::max(diagnosticParts,u32(bin->bytes.data[0x19])+2);
        }
        // Explicit harness capacity, not the production CONS/SMD room count.
        engine.configureModelPools(1,diagnosticParts);
        if (i != 5)
            std::cout << ',';
        std::cout << "{\"path\":\"" << argv[i] << "\",\"sha256\":\"" << hash(raw)
                  << "\",\"room\":\"" << package.room << "\",\"number\":\"" << package.number
                  << "\",\"packet_offset\":" << package.packetOffset
                  << ",\"packet_size\":" << package.packetSize << ",\"bins\":[";
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
            auto *bin=findBin(ref.bin),*tpl=findBin(ref.tpl);
            if(!bin || !tpl) { std::cout << ",\"status\":\"external-resource-not-bound\"}"; continue; }
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
int main(int argc, char **argv) {
    try {
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
