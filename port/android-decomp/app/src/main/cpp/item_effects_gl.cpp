#include "item_effects_gl.h"
#include "actor_gl.h"
#include "world_effects_gl.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace re4 {
namespace {
const ItemEffects *owner{};
std::uint64_t signature{};
std::set<const StaticScene*> meshes;
void sync(const ItemEffects &effects) {
  if(owner==&effects && signature==effects.data().sprites.signature && !meshes.empty())return;
  for(const auto *mesh:meshes)DestroyGenericActorGl(mesh);
  meshes.clear();DestroyItemBillboardsGl();
  try {
    CreateItemBillboardsGl(effects.data());
    for(const auto &entry:effects.modelMeshes()) {
      const auto *mesh=&entry.second;
      std::vector<SkinCorner> skin(mesh->vertices.size()/3);
      for(auto &corner:skin)for(auto *weight:{&corner.position,&corner.normal}) {
        weight->count=1;weight->joints[0]=0;weight->rates[0]=1;
      }
      CreateGenericActorGl(mesh,*mesh,skin);meshes.insert(mesh);
    }
    owner=&effects;signature=effects.data().sprites.signature;
  }catch(...) {DestroyItemBillboardsGl();for(const auto *mesh:meshes)DestroyGenericActorGl(mesh);
    meshes.clear();owner=nullptr;signature=0;throw;}
}
}
void CreateItemEffectsGl(const ItemEffects &effects) {sync(effects);}
void DestroyItemEffectsGl() {
  for(const auto *mesh:meshes)DestroyGenericActorGl(mesh);
  meshes.clear();DestroyItemBillboardsGl();owner=nullptr;signature=0;
}
void ForgetItemEffectsGl() {
  // Scene/context owner forgets the generic actor registry as a whole;
  // deleting old names in the newly current context is forbidden.
  meshes.clear();ForgetItemBillboardsGl();owner=nullptr;signature=0;
}
void DrawItemEffectsGl(const float *camera,const std::array<float,3> &center,float scale,
                      const WorldEffectCamera &view,const ItemEffects &effects,bool normalView) {
  sync(effects);auto models=effects.effectModels();
  const auto depth=[&](const ItemEffectModel &m){float z=0;
    for(unsigned k=0;k<3;++k)z+=(m.model.matrix[k*4+3]-view.eye[k])*view.forward[k];
    return z;};
  std::stable_sort(models.begin(),models.end(),[&](const auto &a,const auto &b){return depth(a)>depth(b);});
  constexpr Matrix34 identity{1,0,0,0,0,1,0,0,0,0,1,0};
  const std::vector<Matrix34> pose{identity};const std::vector<std::array<float,3>> bind(1);
  for(const auto &m:models) {
    const auto mesh=effects.modelMeshes().find(m.model.model);
    if(mesh==effects.modelMeshes().end())throw std::runtime_error("Item-EFM: Original model is missing");
    GenericActorStyle style;style.color=m.model.color;style.blendMode=m.blend;style.zMode=m.zMode;
    const std::array<float,3> light{m.model.matrix[3],m.model.matrix[7],m.model.matrix[11]};
    DrawGenericActorGl(&mesh->second,camera,center,scale,pose,bind,m.model.matrix,
        normalView,m.model.lightMask,&light,1000,&style);
  }
  DrawItemBillboardsGl(camera,center,scale,effects.quads(view));
}
} // namespace re4
