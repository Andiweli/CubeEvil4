#pragma once
#include "player_original.h"
#include "re4_player_live_types.h"
#include "motion_original.h"
#include "rnd.h"
namespace re4::port {
struct ShapeData {u16 nFrame{};u8 num{};ResourceLease source;};
int ShapeMove(ModelInfoView*);int SetShape(ModelInfoView*,f32,ShapeData*);
void PartsWorldPosCalc(ModelCpu*);
f32 GetXZAngle(Vec*,Vec*);f32 GetXZAngleLocal(Vec*,Vec*,f32);f32 GetDistance(Vec*,Vec*);
void playerActorCollision(PlayerDomain*,ModelCpu*);
void AddSpeed(ModelCpu*,const Vec*);void low_RotMatrix(Mtx,Vec*);f32 Muku3(f32,Vec*,f32);
int Front_check(ModelCpu*,Vec*,f32);
[[noreturn]] inline u32 SndCall(u16,u32,Vec*,u32,int,u32){throw MissingGameConsumer("SndCall/original sound request consumer");}
[[noreturn]] inline void AddSandPower(Vec,f32){throw MissingGameConsumer("AddSandPower/original sand effect owner");}
[[noreturn]] inline int GetWaterHeight(Vec*,f32*){throw MissingGameConsumer("GetWaterHeight/original water generator registry");}
[[noreturn]] inline void PlWaterProc(PlayerDomain*){throw MissingGameConsumer("PlWaterProc/original room water effects");}
} // namespace re4::port
