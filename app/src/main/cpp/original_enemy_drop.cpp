// Frozen upstream em_sub.cpp at 9b76ff003081d6ece8b838453fb8310743a972c7.
// Original GetBulletPoint/GetDropBullet/RandomHandgunAmmo/RandomItemCk bodies.
// Host context replaces the game's pG/ItemMgr globals; decision/Rnd order is
// retained, including source branches for other playable inventory classes.
#include "original_enemy_drop.h"
#include "ports/rnd.h"
#include "asset_reader.h"
#include "item_rules.h"
#include <algorithm>
#include <stdexcept>
namespace re4 {
namespace {
using u8=std::uint8_t; using u32=std::uint32_t; using s16=std::int16_t;
constexpr unsigned SYS_OMAKE_ADA_GAME=1,SYS_OMAKE_ETC_GAME=2;
struct DropGame { unsigned stage_no,pl_type,game_mode,Game_level,pl_life; const EnemyDropContext *context; };
thread_local DropGame *pG{};
thread_local unsigned No_drop_cnt{},No_drop_cnt2{};
unsigned SysFlagChk(DropGame *g,unsigned flag) {
 return flag==SYS_OMAKE_ADA_GAME ? g->context->adaGame : g->context->mercenaries;
}
struct DropItems {
 unsigned bulletNumTotal(unsigned id) const {
  const auto &c=*pG->context;
  switch(id) {case 4:return c.handgunAmmo;case 0x18:return c.shotgunAmmo;
   case 0x20:return c.tmpAmmo;case 0x6a:return c.bowAmmo;default:return 0;}
 }
 unsigned num(unsigned id) const {
  const auto &c=*pG->context;if(id==6)return c.greenHerbs;
  return std::find(c.carriedWeapons.begin(),c.carriedWeapons.end(),id)!=c.carriedWeapons.end();
 }
} ItemMgr;
int GetRecoveryPoint() {return int(pG->context->recoveryPoints);}
int GetBulletPoint()
{
    u32 n;
    int pt;

    n = ItemMgr.bulletNumTotal(4);
    if (pG->stage_no <= 1) {
        pt = n;
    } else {
        pt = n / 2 + 1;
    }
    pt += ItemMgr.bulletNumTotal(0x18) * 2;
    n = ItemMgr.bulletNumTotal(0x20);
    pt += n / 5 + 1;
    n = ItemMgr.bulletNumTotal(0x6A);
    pt += n / 5 + 1;
    ItemMgr.bulletNumTotal(0x72);
    return pt + 5;
}

void GetDropBullet(int* ret_id, int* ret_num)
{
    int i = 4;
    int n = 0;  // also holds the handgun ammo total in the third branch (its register: r31 there)
    u8 r;
    u32 f;

    // The bit test as a variable: the `andis.` result stays (cse later reuses it as the zero stored
    // for `*num = 0` on the other path); a plain `flags & 0x80000000` folds to a signed compare.
    f = SysFlagChk(pG, SYS_OMAKE_ADA_GAME);
    if (f) {
        r = Rnd() % 100;
        if (r <= 0x27) {
            *ret_id = i;
            if (Rnd() % 10 > 6) {
                *ret_num = 15;
            } else {
                *ret_num = 10;
            }
            return;
        }
        if (r <= 0x59) {
            i = 0x20;
            n = 25;
            if (Rnd() % 10 > 5) {
                n = 0;
            }
            *ret_id = i;
            *ret_num = n;
            return;
        } else if (r <= 0x5E) {
            i = 7;
            n = 3;
            if (Rnd() % 10 > 7) {
                n = 0;
            }
            *ret_id = i;
            *ret_num = n;
            return;
        } else {
            i = 1;
            *ret_id = i;
            *ret_num = n;
            return;
        }
    } else if (SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
        r = Rnd() % 100;
        switch (pG->pl_type) {
        case 0:
        default:
            if (r <= 0x36) {
                *ret_id = 4;
                *ret_num = 0;
                return;
            }
            if (r <= 0x59) {
            // One copy of the handgun-ammo body: the third branch's `r <= 0x13` arm jumps here (the
            // original's three `bne` land on this block; a duplicated body is cross-jumped arm by arm
            // and its head survives).
            ammo18:
                i = 0x18;
                if ((Rnd() & 0xF) != 5) {
                    n = 5;
                    if (Rnd() % 10 > 4) {
                        n = 3;
                    }
                } else {
                    n = 0;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            } else {
                i = 1;
                *ret_id = i;
                *ret_num = 0;
                return;
            }
        case 2:
            if (r <= 0x1D) {
                *ret_id = i;
                *ret_num = n;
                return;
            } else if (r <= 0x40) {
                i = 0x20;
                n = 25;
                if (Rnd() % 10 > 5) {
                    n = 0;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            } else if (r <= 0x54) {
                i = 2;
                n = 0;
                *ret_id = i;
                *ret_num = n;
                return;
            } else {
                i = 7;
                n = 3;
                if (Rnd() % 10 > 7) {
                    n = 0;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            }
        case 3:
            if (r <= 0x4A) {
                i = 0x20;
                n = 25;
                if (Rnd() % 10 > 5) {
                    n = 0;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            } else {
                i = 1;
                *ret_id = i;
                *ret_num = n;
                return;
            }
        case 4:
            if (r <= 0x4A) {
                i = 0x72;
                n = 5;
                if (Rnd() % 10 > 5) {
                    n = 10;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            } else {
                i = 0xE;
                *ret_id = i;
                *ret_num = n;
                return;
            }
        case 5:
            if (r <= 0x18) {
                *ret_id = i;
                *ret_num = n;
                return;
            }
            if (r <= 0x45) {
                i = 0;
                n = (Rnd() % 10 <= 7) ? 2 : 0;
                *ret_id = i;
                *ret_num = n;
            }
            if (r <= 0x4F) {
                i = 7;
                n = 3;
                if (Rnd() % 10 > 7) {
                    n = 0;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            } else if (r <= 0x59) {
                i = 1;
                *ret_id = i;
                *ret_num = 0;
                return;
            } else if (r <= 0x5E) {
                i = 0xE;
                *ret_id = i;
                *ret_num = 0;
                return;
            } else {
                i = 2;
                *ret_id = i;
                *ret_num = 0;
                return;
            }
        }
    } else {
        n = ItemMgr.bulletNumTotal(4);
        r = Rnd() % 100;
        if (r > 0x28) {
            if (pG->game_mode == 1) {
                if (ItemMgr.num(0x2C) || ItemMgr.num(0x2D) || ItemMgr.num(0x94)) {
                    if (Rnd() % 10 > 4) {
                        i = 0x18;
                        if ((Rnd() & 0xF) != 5) {
                            n = 5;
                            if (Rnd() % 10 > 4) {
                                n = 3;
                            }
                        } else {
                            n = 0;
                        }
                        *ret_id = i;
                        *ret_num = n;
                        return;
                    }
                }
            }
            if ((u32) n <= 0x3B) {
                goto fallback;
            }
        }
        r = Rnd() % 100;
        if (r <= 0x13) {
            if (ItemMgr.num(0x2C) || ItemMgr.num(0x2D) || ItemMgr.num(0x94)) {
                goto ammo18;
            }
        }

        if ((u32) (r - 0x14) <= 0x13) {
            if (ItemMgr.num(0x2E) || ItemMgr.num(0x2F)) {
                i = 7;
                n = 3;
                if (Rnd() % 10 > 7) {
                    n = 0;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            }
        }
        if ((u32) (r - 0x28) <= 0x13) {
            if (ItemMgr.num(0x30) || ItemMgr.num(0x31) || ItemMgr.num(0x32) || ItemMgr.num(0x32) || ItemMgr.num(0x33) ||
                ItemMgr.num(0x3E)) {
                i = 0x20;
                n = 25;
                if (Rnd() % 10 > 5) {
                    n = 0;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            }
        }
        if ((u32) (r - 0x3C) <= 0x13) {
            if (ItemMgr.num(0x37)) {
                if (Rnd() % 10 > 1) {
                    i = 0x1A;
                    n = (Rnd() % 10 <= 7) ? 2 : 0;
                    *ret_id = i;
                    *ret_num = n;
                    return;
                }
            }
            if (ItemMgr.num(0x29) || ItemMgr.num(0x2A) || ItemMgr.num(0x2B)) {
                i = 0;
                n = (Rnd() % 10 <= 7) ? 2 : 0;
                *ret_id = i;
                *ret_num = n;
                return;
            }
        }
        if ((u32) (r - 0x50) <= 9) {
            if (ItemMgr.num(0x36) || ItemMgr.num(0xAB)) {
                i = 0x46;
                n = (Rnd() % 10 <= 7) ? 2 : 0;
                *ret_id = i;
                *ret_num = n;
                return;
            }
        }
        if ((u32) (r - 0x5A) <= 9) {
            switch (Rnd() % 3) {
            default:
                i = 1;
                break;
            case 1:
                i = 2;
                break;
            case 2:
                i = 0xE;
                break;
            }
            *ret_id = i;
            // Codeless launder: keeps the zero and its store from being cross-jumped two insns deep
            // into the fallback's else arm (see `fallback:`); the `*num` input is an anti-dependence
            // on the store, so sched2 still issues the `*id` store first.
            int z = 0;
            asm("" : "+r"(z) : "m"(*ret_num));
            *ret_num = z;
            return;
        }
        if (ItemMgr.num(0x2C) || ItemMgr.num(0x2D) || ItemMgr.num(0x94)) {
            if (Rnd() % 10 > 2) {
                i = 0x18;
                if ((Rnd() & 0xF) != 5) {
                    n = 5;
                    if (Rnd() % 10 > 4) {
                        n = 3;
                    }
                } else {
                    n = 0;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            }
        }
        if (ItemMgr.num(0x30) || ItemMgr.num(0x31) || ItemMgr.num(0x32) || ItemMgr.num(0x32) || ItemMgr.num(0x33) ||
            ItemMgr.num(0x3E)) {
            if (Rnd() % 10 > 4) {
                i = 0x20;
                n = 25;
                if (Rnd() % 10 > 5) {
                    n = 0;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            }
        }
        if (ItemMgr.num(0x2E) || ItemMgr.num(0x2F)) {
            if (Rnd() % 10 > 4) {
                i = 7;
                n = 3;
                if (Rnd() % 10 > 7) {
                    n = 0;
                }
                *ret_id = i;
                *ret_num = n;
                return;
            }
        }
        if (ItemMgr.num(0x36)) {
            if (Rnd() % 10 > 4) {
                i = 0x46;
                n = (Rnd() % 10 <= 7) ? 4 : 0;
                *ret_id = i;
                *ret_num = n;
                return;
            }
        }
        if (ItemMgr.num(0x29) || ItemMgr.num(0x2A) || ItemMgr.num(0x2B) || ItemMgr.num(0x37)) {
            if (ItemMgr.num(0x37)) {
                if (Rnd() % 10 > 4) {
                    i = 0x1A;
                    n = (Rnd() % 10 <= 7) ? 2 : 0;
                    *ret_id = i;
                    *ret_num = n;
                    return;
                }
            }
            if (ItemMgr.num(0x29) || ItemMgr.num(0x2A) || ItemMgr.num(0x2B)) {
                if (Rnd() % 10 > 4) {
                    i = 0;
                    n = (Rnd() % 10 <= 7) ? 2 : 0;
                    *ret_id = i;
                    *ret_num = n;
                    return;
                }
            }
        }
        if (Rnd() % 10 > 7) {
            switch (Rnd() % 3) {
            default:
                i = 1;
                break;
            case 1:
                i = 2;
                break;
            case 2:
                i = 0xE;
                break;
            }
            *ret_id = i;
            // Codeless launder: keeps the zero and its store from being cross-jumped two insns deep
            // into the fallback's else arm (see `fallback:`); the `*num` input is an anti-dependence
            // on the store, so sched2 still issues the `*id` store first.
            int z = 0;
            asm("" : "+r"(z) : "m"(*ret_num));
            *ret_num = z;
            return;
        }
    fallback:
        // `li r0,0; ble; li r0,0x14; stw`: jump.c's post-reload "if (..) x = a; else x = b" hoist, after
        // the then-arm's store cross-jumped into the else arm's store (a fresh label, so the two
        // `Rnd() % 3` sites above keep their own `li r0,0`). A plain `*num = 0` there is matched two
        // insns deep by those sites first (their `li r0,0; stw` tails), which pins their jumps on the
        // else label and blocks the hoist; the codeless launder between each site's zero and its
        // store (see `z` above) is the insn find_cross_jump compares against this `li`, so the sites
        // match one insn (the store) only. COMPILER-DIFF: candidate (jump2 cross-jump order vs the
        // x = a / x = b hoist)
        *ret_id = 4;
        if (pG->stage_no > 1) {
            *ret_num = 20;
        } else {
            *ret_num = 0;
        }
    }
}

static inline void RandomHandgunAmmo(u32& num, int base, int big)
{
    num = (u8) (Rnd() % 6) + ((u8) (Rnd() % 6) + base) + (u8) (Rnd() % 6) + (u8) (Rnd() % 6);
    num *= 5;
    num = num / 10 * 10;
    if ((Rnd() & 0x3F) == 0x1E) {
        num = big;
        if (Rnd() & 0xF) {
            num = 330;
        }
    }
}

int RandomItemCk(int em_id, int* ret_id, int* ret_num, int ctrl_flag)
{
    u8 r;
    u8 r0;
    int bullet;
    int recov;
    int itemId;
    u32 num;
    u32 lim;

    r = Rnd() % 100;
    bullet = GetBulletPoint();
    recov = GetRecoveryPoint();
    switch (em_id) {
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
    case 0x20:
    case 0x22:
    case 0x36:
        if (r <= 0x13) {
            if (SysFlagChk(pG, SYS_OMAKE_ADA_GAME)) {
                return 0;
            }
            if (SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
                return 0;
            }
            switch (em_id) {
            case 0x11:
            case 0x14:
            case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F: case 0x20:
            case 0x22:
            case 0x36:
                itemId = 0x78;
                RandomHandgunAmmo(num, 20, 1980);
                *ret_id = itemId;
                *ret_num = num;
                return 1;
            default:
                itemId = 0x78;
                RandomHandgunAmmo(num, 10, 990);
                *ret_id = itemId;
                *ret_num = num;
                return 1;
            }
        }
        /* fallthrough */
    case 0x25:
        if (r <= 0x3B || (No_drop_cnt > 2 && Rnd() % 10 > 4) || No_drop_cnt > 5 || (ctrl_flag & 1)) {
            No_drop_cnt = 0;
        } else {
            No_drop_cnt++;
            return 0;
        }
        break;
    case 0x23:
        itemId = 0x78;
        num = (u8) (Rnd() % 3) * 10 + 20;
        if ((Rnd() & 3) == 0) {
            if (Rnd() & 1) {
                num = (Rnd() & 1) * 10 + 10;
            } else {
                num = (Rnd() & 1) * 10 + 40;
            }
        }
        *ret_id = itemId;
        *ret_num = num;
        return 1;
    case 0x3C:
        if (r > 0x28) {
            return 0;
        }
        break;
    case 0x2D:
        if (r > 0x3C) {
            r0 = Rnd() % 100;
            itemId = 0xB9;
            if (r0 <= 0x31) {
                itemId = 0xBA;
            }
            if (r0 <= 0xC) {
                itemId = 0xBB;
            }
            *ret_id = itemId;
            num = 1;
            *ret_num = num;
            return 1;
        }
        break;
    case 0x3A:
        break;
    default:
        return 0;
    }
    // The bullet thresholds go through the `lim` variable: a literal `< 30` is folded to `<= 29`
    // (`cmplwi 0x1d; bgt`), the variable keeps `cmplwi 0x1e; bge`.
    lim = 30;
    if (pG->pl_type != 1 && (u32) bullet < lim && Rnd() % 10 > 4) {
        GetDropBullet(ret_id, ret_num);
        return 1;
    }
    lim = 1;
    if (pG->Game_level <= 2) {
        lim = 3;
    }
    if (pG->Game_level > 7) {
        lim = 0;
    }
    if ((u32) recov <= lim) {
        if (recov == 0) {
            No_drop_cnt2++;
        }
        if (Rnd() % 10 > 7 || No_drop_cnt2 > 2) {
            if ((s16) pG->pl_life <= 500) {
                itemId = 5;
                if (Rnd() % 100 > 0x18) {
                    itemId = 6;
                }
            } else {
                itemId = 6;
                if (ItemMgr.num(6) != 0 && Rnd() % 100 > 0x4A) {
                    itemId = 0x19;
                }
            }
            num = 0;
            No_drop_cnt2 = 0;
            *ret_id = itemId;
            *ret_num = num;
            return 1;
        }
    }
    if (pG->pl_type == 1) {
        return 0;
    }
    lim = 0x96;
    if ((u32) bullet < lim) {
        GetDropBullet(ret_id, ret_num);
        return 1;
    }
    if (em_id == 0x2D) {
        // The else arm starts with the Rnd() call: with a plain `itemId = 0xBB` first, jump.c hoists
        // the `itemId = 0xB9` of the then arm above the test.
        if (Rnd() & 3) {
            itemId = 0xB9;
        } else if (Rnd() & 3) {
            itemId = 0xBA;
        } else {
            itemId = 0xBB;
        }
        *ret_id = itemId;
        num = 1;
        *ret_num = num;
        return 1;
    }
    if (ctrl_flag & 1) {
        itemId = 0x78;
        RandomHandgunAmmo(num, 20, 990);
        *ret_id = itemId;
        *ret_num = num;
        return 1;
    }
    return 0;
}

} // namespace
std::optional<EnemyDropChoice> OriginalEnemyRandomDrop(unsigned enemyId,
    const EnemyDropContext &context,EnemyDropState &state,unsigned controlFlags) {
 if(enemyId>255 || context.health>32767 || context.gameLevel>10 ||
    context.stage>7 || context.handgunAmmo>65535 || context.shotgunAmmo>65535 ||
    context.tmpAmmo>65535 || context.bowAmmo>65535 || context.recoveryPoints>65535 ||
    context.greenHerbs>65535 || context.playerType>5 || context.gameMode>1 ||
    state.noDropCount>6 || state.noRecoveryDropCount>2)
  throw std::runtime_error("Enemy drop: invalid original state/context");
 const auto previousRandom=RndState();const auto previousGame=pG;
 const auto previousNoDrop=No_drop_cnt,previousNoRecovery=No_drop_cnt2;
 DropGame game{context.stage,context.playerType,context.gameMode,context.gameLevel,
               context.health,&context};pG=&game;
 No_drop_cnt=state.noDropCount;No_drop_cnt2=state.noRecoveryDropCount;
 RndInit(state.randomState);int id=0,num=0;
 const int result=RandomItemCk(int(enemyId),&id,&num,int(controlFlags));
 state.randomState=RndState();state.noDropCount=No_drop_cnt;
 state.noRecoveryDropCount=No_drop_cnt2;
 RndInit(previousRandom);pG=previousGame;
 No_drop_cnt=previousNoDrop;No_drop_cnt2=previousNoRecovery;
 if(!result)return {};
 return EnemyDropChoice{unsigned(id),unsigned(num)};
}

std::optional<AuthoredEnemyDrop> FindAuthoredEnemyDrop(const RoomArchive &archive,
    unsigned listIndex,const ScenarioState &scenario,std::uint16_t roomId,
    unsigned playerType) {
 if(listIndex>255 || roomId>0x7ff || playerType>5)
  throw std::runtime_error("Enemy drop: invalid authored owner");
 // EmSetEvent's source emset_no0xFF has no ESL owner or linked ITA record.
 // Its first-house Ganado still follows EmSetDropItem's normal random path.
 if(listIndex==0xff)return {};
 const auto *entry=archive.find("ITA");if(!entry)return {};
 const auto bytes=archive.copy(entry-archive.entries().data());const AssetReader r{bytes};
 if(r.u32(0)!=0x49544100 || r.u16(4)!=0x105 || r.u16(6)>256)
  throw std::runtime_error("Enemy drop: invalid GC ITA table");
 r.range(16,std::size_t(r.u16(6))*156);
 std::optional<AuthoredEnemyDrop> result;
 for(unsigned index=0;index<r.u16(6);++index) {
  const auto p=16+std::size_t(index)*156;
  // sceAtLink_check's waiting1 is the enemy-list owner; waiting2 is ETC.
  if(bytes[p+0x35]!=3 || bytes[p+0x46]!=1 || bytes[p+0x47]!=listIndex || (bytes[p+0x84]&1))continue;
  // Source country bit0 excludes USA. An unsupported live callback/parent
  // is rejected, so it cannot become an unconditional reward.
  if(bytes[p+0x52]&1)continue;
  if(r.u32(p+0x3c)||r.u32(p+0x40)||r.u32(p+0x4c))
   throw std::runtime_error("Enemy drop: linked item callback/parent unported");
  const unsigned player=bytes[p+0x82],mask=playerType==0?1:2;
  if(player && !(player&mask))continue;
  AuthoredEnemyDrop drop;drop.listIndex=listIndex;drop.recordIndex=index;
  drop.area=bytes[p+0x36]+0x80;drop.id=r.u16(p+0x78);drop.count=r.u16(p+0x7c);
  drop.itemFlag=r.u16(p+0x7a);drop.autoFlag=r.u16(p+0x7e);
  drop.controlFlags=bytes[p+0x84];drop.effect=bytes[p+0x80];drop.room=roomId;
  drop.position={r.f32(p+0x5c),r.f32(p+0x60),r.f32(p+0x64)};
  drop.fixedPosition=(drop.controlFlags&1)!=0;
  if(drop.itemFlag>=128 || drop.autoFlag>=128)
   throw std::runtime_error("Enemy drop: linked item flag outside native source table");
  if(drop.id>255 && (drop.id<0x1001 || drop.id>0x1005))
   throw std::runtime_error("Enemy drop: linked item constructor unported");
  drop.taken=drop.itemFlag?scenario.globalItems.test(drop.itemFlag):
             drop.autoFlag?scenario.room(roomId).items.test(drop.autoFlag):false;
  std::copy_n(bytes.begin()+p,156,drop.raw.begin());result=std::move(drop);
 }
 return result;
}
bool ClaimAuthoredEnemyDrop(const AuthoredEnemyDrop &drop,ScenarioState &scenario) {
 if(drop.itemFlag>=128||drop.autoFlag>=128)throw std::runtime_error("Enemy drop: invalid claim flag");
 if(drop.itemFlag) {
  if(scenario.globalItems.test(drop.itemFlag))return false;
  scenario.globalItems.set(drop.itemFlag);
 }else if(drop.autoFlag) {
  auto &flags=scenario.room(drop.room).items;if(flags.test(drop.autoFlag))return false;
  flags.set(drop.autoFlag);
 }
 return true;
}
std::optional<EnemyDropChoice> ResolveAuthoredEnemyDrop(const AuthoredEnemyDrop &drop,
    const EnemyDropContext &context,EnemyDropState &state) {
 unsigned id=drop.id,count=drop.count;
 if(id==0x1001 || id==0x1002)return OriginalEnemyRandomDrop(0x10,context,state,id==0x1002?1:0);
 if(id==0x1003 || id==0x1004 || id==0x1005) {
  // Source SceAtCheckSystemItemSet normal Leon branches.
  if(context.playerType!=0)throw std::runtime_error("Enemy drop: linked system-item character unported");
  count=id==0x1003?15:id==0x1004?10:20;id=id==0x1005?4:0x18;
 }
 if(!OriginalItemInfo(id).type)throw std::runtime_error("Enemy drop: unknown authored item id");
 return EnemyDropChoice{id,count};
}
} // namespace re4
