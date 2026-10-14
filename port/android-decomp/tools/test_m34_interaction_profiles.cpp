// Data-backed construction coverage across every decoded original room.
// This verifies source profile parsing; it does not claim every event plays.
#include "room_interactions.h"
#include "object_actions.h"
#include "room_pickups.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
namespace {
std::vector<std::uint8_t> entry(const re4::RoomArchive &archive,const char *tag) {
  const auto *e=archive.find(tag);
  return e?archive.copy(e-archive.entries().data()):std::vector<std::uint8_t>{};
}
std::string quoted(const std::string &input) {
  std::string output="\"";
  for(auto c:input) {
    if(c=='"'||c=='\\') output+='\\';
    if(c=='\n') output+="\\n";
    else output+=c;
  }
  return output+'"';
}
bool roomName(const std::string &name) {
  return name.size()==4&&name[0]=='r'&&std::all_of(name.begin()+1,name.end(),
      [](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
unsigned flagChecks{};
void require(bool value,const char *message) {
  if(!value)throw std::runtime_error(message);
  ++flagChecks;
}
void breakFixture(re4::ObjectActions &actions,std::size_t index,unsigned serial) {
  const auto &object=actions.objects().at(index);
  const auto from=re4::TransformPoint(object.matrix,{0,object.shapes[0].height*.5f,-2000});
  const auto to=re4::TransformPoint(object.matrix,{0,object.shapes[0].height*.5f,2000});
  const auto hit=re4::HitAuthoredShapes(object.shapes,{},object.matrix,
      {from[0],from[1],from[2]},{to[0],to[1],to[2]},1e16f,1);
  require(hit.has_value(),"Actual source flag container has its original authored weapon shape");
  require(actions.applyShot({index,object.placement.slot,*hit},serial),"Actual source flag container breaks normally");
}
std::size_t flaggedItem(const re4::ObjectActions &actions,unsigned flag,unsigned id) {
  const auto &objects=actions.objects();
  const auto found=std::find_if(objects.begin(),objects.end(),[&](const auto &o){return o.hasItem&&o.item.itemFlag==flag&&o.item.id==id;});
  require(found!=objects.end(),"Actual original room retains its globally flagged bound container item");
  return std::size_t(found-objects.begin());
}
void actualFlagFixtures(const std::filesystem::path &decoded) {
  const auto load=[&](const char *name,unsigned room,re4::ScenarioState &state) {
    const auto archive=re4::RoomArchive::Load((decoded/(std::string(name)+".dat")).string());
    return re4::ObjectActions(room,archive,re4::ReadRoomObjectPlacements(entry(archive,"ETS")),nullptr,&state);
  };
  re4::ScenarioState state;
  auto first=load("r103",0x103,state),duplicate=load("r113",0x113,state);
  const auto a=flaggedItem(first,7,0x5e),b=flaggedItem(duplicate,7,0x5e);
  require(first.objects()[a].item.recordIndex==9 && first.objects()[a].placement.slot==5 &&
          duplicate.objects()[b].item.recordIndex==0 && duplicate.objects()[b].placement.slot==5,
          "Real r103 ITA9/ETS5 and r113 ITA0/ETS5 share original globalflag7");
  breakFixture(first,a,1);breakFixture(duplicate,b,1);
  const auto stale=duplicate.snapshot();
  require(first.itemVisible(a)&&duplicate.itemVisible(b),"Both actual-room global copies start untaken");
  require(first.takePickup(a).has_value(),"First actual original globally flagged item can be taken");
  require(state.globalItems.test(7)&&!state.globalItemsFound.test(7),"Actual ordinary acquisition commits taken and preserves source found flags");
  require(!duplicate.itemVisible(b)&&!duplicate.takePickup(b),"Other actual room cannot duplicate shared original quest treasure");
  duplicate.restore(stale);
  require(!duplicate.itemVisible(b)&&!duplicate.takePickup(b),"Restoring an actual old room snapshot cannot bypass global itemFlag");
  for(auto fixture:{std::array<unsigned,3>{0x10d,24,0xc7},
                    std::array<unsigned,3>{0x11d,21,0xc8},
                    std::array<unsigned,3>{0x306,55,0x59}}) {
    std::ostringstream name;name<<'r'<<std::hex<<fixture[0];
    auto actions=load(name.str().c_str(),fixture[0],state);
    const auto index=flaggedItem(actions,fixture[1],fixture[2]);
    breakFixture(actions,index,1);
    require(actions.takePickup(index).has_value(),"Actual bound globally flagged original treasure can be acquired");
    require(state.globalItems.test(fixture[1])&&!state.globalItemsFound.test(fixture[1]),"Each actual original bound-item taken flag commits without marking shoot-down found");
    auto reloaded=load(name.str().c_str(),fixture[0],state);
    const auto again=flaggedItem(reloaded,fixture[1],fixture[2]);
    breakFixture(reloaded,again,2);
    require(!reloaded.itemVisible(again)&&!reloaded.takePickup(again),"Reloaded original container remains destructible without duplicating treasure");
  }
}
}
int main(int argc,char **argv) {
  try {
    if(argc!=3) throw std::runtime_error("Usage: m34_interaction_profiles_check DECODED_ROOMS PL00");
    std::vector<std::filesystem::path> paths;
    for(const auto &file:std::filesystem::directory_iterator(argv[1]))
      if(file.path().extension()==".dat"&&roomName(file.path().stem().string()))paths.push_back(file.path());
    std::sort(paths.begin(),paths.end());
    std::cout<<"{\"scope\":\"Native constructor coverage, not animation/event equivalence\",\"room_count\":"<<paths.size()<<",\"rooms\":[\n";
    unsigned rooms=0,modelChecks=0,totalBoxes=0,totalBarrels=0,totalPickups=0,totalWindows=0,totalDoors=0,
             totalModelOwners=0,totalRenderedVariants=0,totalMissingVariants=0;
    for(const auto &path:paths) {
      const auto name=path.stem().string();
      const auto id=unsigned(std::stoul(name.substr(1),nullptr,16));
      const auto archive=re4::RoomArchive::Load(path.string());
      const auto ets=entry(archive,"ETS");
      const auto placements=ets.empty()?std::vector<re4::RoomObjectPlacement>{}:re4::ReadRoomObjectPlacements(ets);
      re4::ScenarioState scenario;
      if(rooms++)std::cout<<",\n";
      std::cout<<"{\"room\":"<<quoted(name)<<",\"ets\":"<<placements.size();
      try {
        re4::ObjectActions actions(id,archive,placements,nullptr,&scenario);
        unsigned boxes=0,barrels=0,dropModels=0;
        for(const auto &object:actions.objects()){boxes+=!object.explosive;barrels+=object.explosive;dropModels+=object.hasItem;}
        totalBoxes+=boxes;totalBarrels+=barrels;
        std::cout<<",\"containers\":{\"boxes\":"<<boxes<<",\"explosive_barrels\":"<<barrels<<",\"bound_drops\":"<<dropModels<<"}";
        const auto before=actions.snapshot().checksum;
        const auto layer=re4::BuildRoomItems(archive,actions);
        if(actions.snapshot().checksum!=before)
          throw std::runtime_error("All-room item model generation changed source RNG or transactions");
        ++modelChecks;
        const auto missing=layer.placements.size()-layer.rendered;
        totalModelOwners+=layer.owners;totalRenderedVariants+=layer.rendered;totalMissingVariants+=missing;
        std::cout<<",\"container_item_models\":{\"owners\":"<<layer.owners<<",\"rendered_variants\":"<<layer.rendered<<",\"missing_model_variants\":"<<missing<<"}";
      }catch(const std::exception &e){std::cout<<",\"container_error\":"<<quoted(e.what());}
      try {
        re4::RoomPickups pickups(id,archive,scenario);
        totalPickups+=pickups.items().size();
        std::cout<<",\"pickups\":{\"eligible\":"<<pickups.items().size()<<",\"describe\":"<<quoted(pickups.describe())<<"}";
      }catch(const std::exception &e){std::cout<<",\"pickup_error\":"<<quoted(e.what());}
      const bool supported=std::any_of(placements.begin(),placements.end(),[](const auto &p){return p.id==0||p.id==7||p.id==0x25||p.id==3||p.id==0xd;});
      if(supported)try {
        re4::RoomInteractions interior(id,archive,placements,argv[2],static_cast<const re4::ScenarioState&>(scenario).room(id));
        totalWindows+=interior.windows().size();totalDoors+=interior.doors().size();
        std::cout<<",\"interior\":{\"windows\":"<<interior.windows().size()<<",\"doors\":"<<interior.doors().size()<<",\"racks\":"<<interior.racks().size()<<"}";
      }catch(const std::exception &e){std::cout<<",\"interior_error\":"<<quoted(e.what());}
      std::cout<<"}";
    }
    actualFlagFixtures(argv[1]);
    std::cout<<"\n],\"actual_original_item_flag_checks\":"<<flagChecks<<",\"item_model_generation_checks\":"<<modelChecks<<",\"totals\":{\"boxes\":"<<totalBoxes<<",\"explosive_barrels\":"<<totalBarrels<<",\"eligible_pickups\":"<<totalPickups<<",\"windows\":"<<totalWindows<<",\"doors\":"<<totalDoors<<",\"container_model_owners\":"<<totalModelOwners<<",\"rendered_container_item_variants\":"<<totalRenderedVariants<<",\"missing_container_item_model_variants\":"<<totalMissingVariants<<"}}\n";
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
