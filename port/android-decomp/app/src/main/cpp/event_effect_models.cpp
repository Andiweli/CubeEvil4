#include "event_playback.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace re4 {
void OriginalEvent::prepareRoomEffectModels(const Smd &room,const Smd *common) {
  effectModels_.clear();std::set<unsigned> ids;
  for(const auto &target:effectsData_.sprites.targets)for(const auto &record:target.records)
    if(record.enabled && record.bytes[1]==0xfc)ids.insert(record.bytes[2]);
  if(ids.empty())return;
  Smd resource=room;
  for(const auto id:ids) {
    const auto head=std::find_if(room.placements.begin(),room.placements.end(),
        [&](const auto &p){return p.work==id;});
    if(head==room.placements.end())throw std::runtime_error("EVD EFM: Original SMD group missing: "+std::to_string(id));
    auto &p=resource.placements[std::size_t(head-room.placements.begin())];
    // EfmSeqSet copies the head object's BIN/TPL addresses, then modelInit
    // installs a fresh bind pose. The authored scroll placement is not copied.
    p.position={};p.rotation={};p.scale={1,1,1};p.motion=0xff;
    auto mesh=BuildStaticScene(resource,int(head-room.placements.begin()),common);
    // Fresh EFM04 differs from setObj: its LightInfo offset is
    // bound.offset - pList->pos. Adding the root part puts the box at
    // the original BIN bound, not at bound + root a second time.
    for(auto &draw:mesh.draws)
      for(unsigned axis=0;axis<3;++axis)draw.lightVolume.center[axis]-=draw.sortCenter[axis];
    AttachSceneTextures(resource,mesh,common);
    if(mesh.vertices.empty() || mesh.included!=1 || mesh.skipped || mesh.fallbackTriangles || mesh.unresolvedAlphaTriangles)
      throw std::runtime_error("EVD EFM: Incomplete original group model: "+std::to_string(id));
    effectModels_.emplace(id,std::move(mesh));
  }
}
}
