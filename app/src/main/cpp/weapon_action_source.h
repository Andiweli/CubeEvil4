// Generated from frozen weapon controllers, pl_class, objWep and em_dm_val.
#pragma once
#include <array>
#include "weapon_carry_source.h"
namespace re4 {
enum class WeaponFamily { Handgun, Shotgun, Rifle, Machine, Rocket, Mine, Matilda, Grenade, Bow };
struct WeaponActionSource { unsigned number; WeaponFamily family; std::array<unsigned,9> player; std::array<unsigned,6> object; unsigned drawReady,fireReady,reloadCommit,reloadRelease; std::array<float,3> marker; };
inline const WeaponActionSource *OriginalWeaponAction(unsigned number) {
 static constexpr WeaponActionSource data[]{
  {1,WeaponFamily::Handgun,{34,36,38,39,40,41,42,43,45},{52,50,54,57,55,51},4,12,31,36,{262.0f, -24.0f, 32.0f}},
  {2,WeaponFamily::Handgun,{34,36,38,39,40,41,42,43,45},{54,52,56,59,57,53},4,12,31,36,{228.0f, -24.0f, 29.0f}},
  {3,WeaponFamily::Handgun,{34,36,38,39,40,41,42,43,45},{52,50,54,56,55,51},4,14,44,60,{156.0f, -6.0f, 90.0f}},
  {4,WeaponFamily::Handgun,{34,36,38,39,40,41,42,43,45},{52,50,54,57,55,51},4,12,31,38,{231.0f, -24.0f, 30.5f}},
  {5,WeaponFamily::Handgun,{34,36,38,39,40,41,42,43,45},{52,50,51,52,50,51},4,19,75,97,{378.5f, -1.7f, 61.7f}},
  {6,WeaponFamily::Handgun,{34,36,38,39,40,41,42,43,45},{52,50,54,58,53,51},4,19,44,46,{192.0f, -24.0f, 134.0f}},
  {7,WeaponFamily::Shotgun,{24,31,26,32,34,30,33,35,42},{49,48,43,49,50,43},10,40,30,73,{-116.0f, -21.0f, 32.0f}},
  {8,WeaponFamily::Shotgun,{24,31,26,32,34,30,33,35,42},{49,48,43,49,48,43},8,32,35,70,{-129.0f, -23.0f, 62.0f}},
  {9,WeaponFamily::Rifle,{20,22,21,21,21,21,21,21,23},{35,35,34,35,35,34},4,20,10,108,{228.0f, -24.0f, 29.0f}},
  {10,WeaponFamily::Rifle,{20,22,21,21,21,21,21,21,23},{34,34,33,34,34,33},4,43,34,57,{228.0f, -24.0f, 29.0f}},
  {11,WeaponFamily::Machine,{26,29,27,31,33,28,32,34,30},{42,39,41,47,45,40},5,3,40,70,{206.0f, -24.0f, 33.4f}},
  {12,WeaponFamily::Machine,{26,29,27,31,33,28,32,34,30},{41,39,40,41,42,40},5,3,40,85,{4.0f, -24.0f, 85.0f}},
  {13,WeaponFamily::Rocket,{24,25,15,18,20,17,19,21,24},{29,29,29,29,29,29},4,72,0,1,{265.7f, 130.0f, 73.3f}},
  {14,WeaponFamily::Mine,{18,21,19,23,25,20,24,26,22},{33,30,31,33,30,31},4,40,74,103,{-97.0f, 7.0f, -18.0f}},
  {15,WeaponFamily::Handgun,{34,36,38,39,40,41,42,43,45},{52,50,51,52,50,51},4,33,34,96,{87.70001f, -23.1f, 72.6f}},
  {17,WeaponFamily::Matilda,{17,21,18,23,25,61,62,63,64},{54,53,38,56,55,37},4,11,31,36,{225.6f, -24.0f, 40.1f}},
  {19,WeaponFamily::Grenade,{15,16,17,20,23,18,21,24,15},{0,0,0,0,0,0},4,30,0,1,{500.0f, 0.0f, 0.0f}},
  {22,WeaponFamily::Grenade,{15,16,17,20,23,18,21,24,15},{0,0,0,0,0,0},4,30,0,1,{500.0f, 0.0f, 0.0f}},
  {23,WeaponFamily::Grenade,{15,16,17,20,23,18,21,24,15},{0,0,0,0,0,0},4,30,0,1,{500.0f, 0.0f, 0.0f}},
  {28,WeaponFamily::Bow,{31,32,33,36,39,34,37,40,31},{45,47,45,45,44,45},4,35,0,1,{-400.0f, -6.2f, 163.7f}},
  {33,WeaponFamily::Shotgun,{24,31,26,32,34,30,33,35,42},{49,48,43,49,50,43},10,40,30,73,{-300.0f, -21.0f, 32.0f}},
 };
 for(const auto &v:data)if(v.number==number)return &v;
 return nullptr;
}
inline unsigned OriginalWeaponNumber(unsigned item) {
 const auto *v=OriginalWeaponCarry(item); return v?v->number:0;
}
inline bool OriginalShotgun(unsigned number) {return number==7 || number==8 || number==33;}
inline unsigned OriginalShotTargets(unsigned number) {return number==1?2:number==5 || number==6?3:number==9 || number==10 || number==15?5:1;}
inline unsigned OriginalWeaponSize(unsigned number) {
 switch(number) {
 case 5:case 6:case 7:case 8:case 9:case 10:case 13:case 14:case 15:case 18:case 19:case 21:case 22:case 30:case 33:case 40:case 41:case 44:return 1;
 case 16:case 26:return 2;
 case 0:case 20:case 34:case 35:case 36:case 37:return 3;
 default:return 0;
 }
}
inline constexpr unsigned OriginalGanadoBaseDamage[46] = {0, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 9999, 0, 150,
    100, 150, 9999, 2500, 200, 500, 0, 0, 100, 0, 150, 150, 150, 150, 150, 0,
    0, 150, 0, 0, 400, 2000, 150, 150, 150, 2500, 0, 150, 150, 2500,};
inline constexpr float OriginalWeaponPower[46][7] = {{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 0.9f, 1.1f, 1.3f, 1.5f, 1.7f, 1.9f, 1.9f },
    { 1.0f, 1.2f, 1.4f, 1.6f, 1.8f, 2.0f, 2.0f },
    { 1.4f, 1.7f, 2.0f, 2.4f, 2.8f, 3.5f, 5.0f },
    { 1.6f, 1.8f, 2.0f, 2.3f, 2.7f, 3.0f, 3.4f },
    { 13.0f, 15.0f, 17.0f, 20.0f, 24.0f, 28.0f, 50.0f },
    { 25.0f, 30.0f, 35.0f, 35.0f, 35.0f, 35.0f, 35.0f },
    { 4.0f, 4.5f, 5.0f, 6.0f, 7.0f, 8.0f, 8.0f },
    { 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 12.0f, 12.0f },
    { 4.0f, 5.0f, 6.0f, 8.0f, 10.0f, 12.0f, 18.0f },
    { 7.0f, 8.0f, 9.0f, 11.0f, 13.0f, 15.0f, 15.0f },
    { 0.4f, 0.5f, 0.6f, 0.8f, 1.0f, 1.2f, 1.8f },
    { 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.2f, 1.4f, 1.6f, 1.8f, 2.0f, 2.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f },
    { 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 50.0f, 50.0f },
    { 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f },
    { 1.0f, 1.2f, 1.4f, 1.6f, 2.0f, 4.0f, 4.0f },
    { 7.0f, 8.0f, 9.0f, 12.0f, 16.0f, 32.0f, 32.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 5.0f, 5.5f, 6.0f, 6.5f, 7.0f, 8.0f, 10.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 4.0f, 5.0f, 6.0f, 8.0f, 10.0f, 12.0f, 18.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },};
inline constexpr float OriginalShotgunFarPower[3][7] = {{ 1.4f, 1.6f, 1.8f, 2.2f, 2.5f, 3.0f, 6.0f },
    { 2.0f, 2.4f, 2.7f, 3.0f, 3.5f, 4.0f, 4.0f },
    { 2.5f, 2.8f, 3.0f, 3.3f, 3.5f, 4.0f, 8.0f },};
} // namespace re4
