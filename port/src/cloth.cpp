#include "cloth_original.h"
#include <stdexcept>
namespace re4::port {
void ClothDomain::validate(ModelCpu *m,const ClothInfo *c) const {
    if(!m || !m->pList || !c || c->Num<0 || c->Num>m->nParts || (c->Num && (!c->pCloth || !c->pParent || !c->pChild)))throw std::invalid_argument("original cloth owner/count");
    for(int i=0;i<c->Num;++i)for(auto t:{c->pCloth,c->pParent,c->pChild,c->pLeft,c->pRight,c->pUpLeft,c->pUpRight})if(t && t[i]!=255 && t[i]>=m->nParts)throw std::out_of_range("original cloth joint reference");
    if(c->At_num<0 || c->At_num>256 || (c->At_num && !c->pAtset))throw std::invalid_argument("original cloth collision volumes");
    for(int i=0;i<c->At_num;++i)if(c->pAtset[i].P1>=m->nParts || c->pAtset[i].P2>=m->nParts)throw std::out_of_range("original cloth volume parent reference");
}
void ClothDomain::initialize(ModelCpu *m){clear();if(pG->pl_costume!=2)PlClothSetLeon(m,&leonHair,&leonJacket,&leonHolster);}
} // namespace re4::port
