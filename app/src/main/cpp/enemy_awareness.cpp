#include "enemy_awareness.h"
#include <cmath>
#include <algorithm>
#include <cstdint>
namespace re4 {
namespace {
using f32=float;using u32=std::uint32_t;
struct Vec {float x{},y{},z{};};
struct FREE_EM10 {unsigned Be_flg{},Goto_mode{};float Pl_rot{},L_pl_route{};};
struct Damage {bool active{};bool isDamage() const{return active;}};
enum {EM_STATUS_ACTIVE=1};
struct cEm10 {
 unsigned type{},flag{},id{},r_no_0{},r_no_1{};int hp{};bool allocated{},active{};
 float l_pl{};Vec pos{},ang{};Damage dmg;FREE_EM10 work;
 bool isAlive()const{return allocated;}
 bool checkStatus(unsigned)const{return active;}
};
using cEm=cEm10;
#define EM10_WK(em) (&(em)->work)
enum {STA_PL_SE_FOOT,STA_SE_BURST,STA_PL_FIRE};
class OriginalAwarenessPort {
 struct Game {bool flags[3]{};struct {unsigned type{};Vec pos{};}SeInfo;} game;
 struct Player {Vec pos{};} player;
 Game *pG=&game;Player *pPL=&player;
 bool somebody{},result{};
 bool StaFlagChk(Game *g,unsigned flag)const{return g->flags[flag];}
 bool em10SomebodyDamageNowCk(cEm10*)const{return somebody;}
 void em10SetRtnFind(cEm10*){result=true;}
int em10FindCk(cEm10* em, int a)
{
    FREE_EM10* w = EM10_WK(em);
    int find = 0;
    int dead;

    if (w->Be_flg & 0x100) {
        return 0;
    }
    if (em->type == 10 || em->type == 13) {
        if (a == 2) {
            return 0;
        }
    }
    if (w->Be_flg & 1) {
        f32 r;
        switch (w->Goto_mode) {
        case 0:
        case 6:
        case 7:
        case 10:
        case 11:
        case 12:
        case 13:
            r = 225000000.0f;
            break;
        default:
            r = 36000000.0f;
            break;
        }
        if (em->l_pl < r) {
            if (w->Pl_rot < 1.0471976f) {
                find = 1;
            }
        }
        if (StaFlagChk(pG, STA_PL_SE_FOOT)) {
            if (em->l_pl < 25000000.0f) {
                find = 1;
            }
        }
        if (em->l_pl < 12250000.0f) {
            find = 1;
        }
        if (a != 0) {
            if (fabsf(em->pos.y - pPL->pos.y) > 2000.0f) {
                find = 0;
            }
        }
    }
    if (em10SomebodyDamageNowCk(em)) {
        find = 1;
    }
    if (!(em->flag & 0x10)) {
        switch (w->Goto_mode) {
        case 0:
        case 6:
        case 7:
        case 10:
        case 11:
        case 12:
        case 13:
            if (StaFlagChk(pG, STA_SE_BURST)) {
                // em3c bell idiom: three identical arms assigning `r` keep the dispatch compares; the
                // override in the distance block makes the arm sets dead (flow deletes them, the
                // compares stay) and `r` a block-local pseudo loaded at the use (local-alloc gives
                // it the next free FPR, f9, and the high r10 because r9/r11 hold pG).
                f32 r;
                switch (pG->SeInfo.type) {
                case 0:
                    r = 25000.0f;
                    break;
                case 1:
                    r = 25000.0f;
                    break;
                default:
                    r = 25000.0f;
                    break;
                }
                {
                    f32 dx = em->pos.x - pG->SeInfo.pos.x;
                    f32 dy = em->pos.y - pG->SeInfo.pos.y;
                    f32 dz = em->pos.z - pG->SeInfo.pos.z;
                    r = 25000.0f;
                    if (dx * dx + dy * dy + dz * dz < r * r) {
                        if ((w->Be_flg & 1) && w->L_pl_route < r) {
                            find = 1;
                        }
                    }
                }
            }
            if (StaFlagChk(pG, STA_PL_FIRE)) {
                if (w->L_pl_route < 25000.0f) {
                    find = 1;
                }
            }
            break;
        }
    }
    dead = em->dmg.isDamage();
    if (dead) {
        find = 1;
    }
    if (em->flag & 0x80) {
        find = 1;
    }
    switch (find) {
    case 0:
        return 0;
    case 1:
        em10SetRtnFind(em);
        return 1;
    default:
        return 0;
    }
}
public:
 bool evaluate(const GanadoAwarenessInput &in,unsigned mode) {
  cEm10 em;em.type=in.type;em.flag=in.flags;em.l_pl=in.distanceSquared;
  em.pos.y=in.heightDelta;em.dmg.active=in.damaged;
  em.work.Be_flg=(in.found?0x100:0)|(in.lineVisible?1:0);
  em.work.Goto_mode=in.gotoMode;em.work.Pl_rot=in.viewAngle;em.work.L_pl_route=in.routeDistance;
  game.flags[STA_PL_SE_FOOT]=in.playerFootstep;game.flags[STA_PL_FIRE]=in.playerFired;
  game.flags[STA_SE_BURST]=in.burst;game.SeInfo.pos={std::sqrt(in.burstDistanceSquared),in.heightDelta,0};
  somebody=in.somebodyDamaged;em10FindCk(&em,int(mode));return result;
 }
};
class OriginalPeerPort {
 struct Game {unsigned stage_no=1,room_no=0;}game;
 Game *pG=&game;
 struct Manager {
  std::vector<cEm10> actors;
  unsigned getArrayNum()const{return unsigned(actors.size());}
  cEm *fastAt(unsigned index){return &actors.at(index);}
 }EmMgr;
 float Muku(const Vec *from,const Vec *to,float heading,float maximum)const {
  return std::clamp(std::remainder(std::atan2(to->x-from->x,to->z-from->z)-heading,6.2831853071795864769f),-maximum,maximum);
 }
int em10SomebodyFindNowCk(cEm10* em)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);
        if (!e->isAlive()) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(EM_STATUS_ACTIVE)) {
            continue;
        }
        if (e->r_no_0 == 1 && e->r_no_1 == 0xD) {
            return 0;
        }
    }
    return 1;
}
int em10SomebodyDamageNowCk(cEm10* em)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);
        int dm;
        if (!e->isAlive()) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(EM_STATUS_ACTIVE)) {
            continue;
        }
        dm = e->r_no_0 == 2;
        if (e->hp <= 0) {
            if (e->r_no_0 != 3) {
                continue;
            }
            if (e->r_no_1 == 3) {
                continue;
            }
            dm = 1;
        }
        if (!dm) {
            continue;
        }
        if (pG->stage_no != 1 || pG->room_no != 1) {
            f32 dx = em->pos.x - e->pos.x;
            f32 dy = em->pos.y - e->pos.y;
            f32 dz = em->pos.z - e->pos.z;
            f32 d = dx * dx + dy * dy + dz * dz;
            if (!(d < 9000000.0f)) {
                if (!(fabsf(Muku(&em->pos, &e->pos, em->ang.y, 3.1415927f)) < 1.0471976f)) {
                    continue;
                }
                if (!(d < 100000000.0f)) {
                    continue;
                }
            }
        }
        return 1;
    }
    return 0;
}
public:
 GanadoPeerAwareness evaluate(const CollisionVec &observer,float heading,const std::vector<GanadoPeerState> &others,unsigned room) {
  game.stage_no=room>>8;game.room_no=room&255;
  cEm10 self;self.pos={float(observer[0]),float(observer[1]),float(observer[2])};self.ang.y=heading;
  for(const auto &in:others) {
   cEm10 em;em.id=in.id;em.pos={float(in.position[0]),float(in.position[1]),float(in.position[2])};
   em.r_no_0=in.routine;em.r_no_1=in.subroutine;em.hp=in.hp;em.allocated=in.allocated;em.active=in.active;
   EmMgr.actors.push_back(em);
  }
  return {em10SomebodyDamageNowCk(&self)!=0,em10SomebodyFindNowCk(&self)==0};
 }
};

#undef EM10_WK
} // namespace
bool OriginalGanadoFind(const GanadoAwarenessInput &input,unsigned mode) {
 return OriginalAwarenessPort{}.evaluate(input,mode);
}
GanadoPeerAwareness OriginalGanadoPeers(const CollisionVec &observer,float heading,
    const std::vector<GanadoPeerState> &others,unsigned room) {
 return OriginalPeerPort{}.evaluate(observer,heading,others,room);
}
} // namespace re4
