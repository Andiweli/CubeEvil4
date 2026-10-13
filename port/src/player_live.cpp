#include "player_live_original.h"
namespace re4::port {
void PlayerDomain::moveMatCalcBefore(){
    auto *p=dynamic_cast<NativePlayerActor*>(actorModel.get());
    if(!p || p->player!=this)throw std::logic_error("native actor pre-matrix dispatch");
    p->moveMatCalcBefore();
}
void PlayerDomain::moveCloth(){
    auto *p=dynamic_cast<NativePlayerActor*>(actorModel.get());
    if(!p || p->player!=this)throw std::logic_error("native actor cloth dispatch");
    p->moveCloth();
}
void PlayerDomain::dispatchRoutine0(){
    if(actorModel->r_no_0>=Pl_func_tbl.size())throw std::out_of_range("original Pl_func_tbl routine index");
    if(!Pl_func_tbl[actorModel->r_no_0])throw MissingGameConsumer(std::string(OriginalPlayerRoutineNames[actorModel->r_no_0])+"/original routine binding");
    Pl_func_tbl[actorModel->r_no_0](this);
}
void PlayerDomain::dispatchRoutine1(){
    if(actorModel->r_no_1==0){pl_R1_Footwork(this);return;}
    if(actorModel->r_no_1==1){pl_R1_Walk(this);return;}
    if(actorModel->r_no_1==2){pl_R1_Back(this);return;}
    if(actorModel->r_no_1>=std::size(OriginalPlayerMoveNames))throw std::out_of_range("original Pl_func_move_tbl routine index");
    throw MissingGameConsumer(std::string(OriginalPlayerMoveNames[actorModel->r_no_1])+"/original movement routine binding");
}
void PlayerDomain::initializeMotionWork(ModelMotion *work,void *fcv,void *seq,int hokan,int flags,int frame){
    if(!modelHandle || !actorModel)throw std::logic_error("player motion owner");
    if(work==&actorModel->Motion)models.initializeMotion(*modelHandle,lease(fcv),seq?lease(seq):ResourceLease{},frame,flags,hokan,false);
    else if(work==&actorModel->m_SubMot){if(seq)throw std::invalid_argument("neck motion has a foreign sequence owner");models.initializeAuxMotion(*modelHandle,*actorModel,lease(fcv),frame,flags,hokan);}
    else throw std::invalid_argument("foreign player motion work");
}
int PlayerDomain::motionMove(){
    if(!runtime || !modelHandle)throw MissingGameConsumer("cPlayer::motionMove/native actor services");
    MotionActorContext context;context.ikOff=actorModel->checkStatus(EM_STATUS_IK_OFF)!=0;context.atariFlags=actorModel->atari.m_flag;
    context.sat=&runtime->sat();
    auto result=models.advanceMotion(*modelHandle,pG->Speed,&context);++construction.motionMoves;return int(result);
}
int PlayerDomain::SatBoundary::check(PlayerDomain *p,int mask){
    if(p!=player || !p->runtime || !p->actorModel)throw std::logic_error("native player SAT owner");
    return p->runtime->sat().checkActor(p->actorModel,mask);
}
int PlayerDomain::SatBoundary::hitCheck(Vec *a,Vec *b,Vec *point,Vec *normal,int flags,int mask){
    if(!player->runtime || !a || !b)throw std::invalid_argument("native player SAT segment");
    auto hit=player->runtime->sat().hitCheck(*a,*b,flags,mask);
    if(hit.attribute){if(point)*point=hit.point;if(normal)*normal=hit.normal;}
    return int(hit.attribute);
}
f32 PlayerDomain::SatBoundary::getFloor(Vec *p,int,f32 above,f32 below,int mask){
    if(!player->runtime || !p)throw std::invalid_argument("native player SAT floor");
    return player->runtime->sat().getFloor(*p,above,below,mask);
}
} // namespace re4::port
