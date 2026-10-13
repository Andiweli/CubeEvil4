#include "player_live_original.h"
#include "action_original.h"
#include <algorithm>
namespace re4::port {
void ActionCallback::invoke(void *arg,std::intptr_t d) const {
    if(owner!=std::this_thread::get_id())throw std::logic_error("original action callback thread owner");
    // All original consumers are owner-thread operations. A task may abandon
    // its stack on cancellation: never put a shared ownership pin on that stack.
    if(lifetime.expired())throw std::logic_error("retired original action callback owner");
    if(!function)throw std::logic_error("empty original action callback");
    function(arg,d);
}
void ActionManager::clearOrdering(){m_ot.fill(-1);for(auto &w:m_task)w=ACT_WORK{};}
void ActionManager::addWork(int slot,ACT_WORK *w){
    auto it=std::find_if(m_task.begin(),m_task.end(),[w](const auto &candidate){return &candidate==w;});
    if(slot<0 || slot>=16 || it==m_task.end())throw std::out_of_range("native action OT insertion");
    w->tag=m_ot[slot];m_ot[slot]=int(it-m_task.begin());
}
ActionManager::ACT_WORK *ActionManager::work(int i){if(i<0 || i>=m_num)throw std::logic_error("invalid native action OT link");return &m_task[i];}
int ActionManager::first() const {for(int slot=15;slot>=0;--slot)if(m_ot[slot]!=-1)return m_ot[slot];return -1;}
int ActionManager::next(const ACT_WORK *w) const {if(w->tag!=-1)return w->tag;for(int slot=int(w->slot)-1;slot>=0;--slot)if(m_ot[slot]!=-1)return m_ot[slot];return -1;}
void ActionManager::set(int k,int s,ActionCallback f,void *a,int flags,int b,int t,std::intptr_t d){
    if(s<0 || s>15 || k<0 || b<0 || b>255 || t<0 || t>2)throw std::invalid_argument("original action registration bounds");
    if(f && f.lifetime.expired())throw std::logic_error("action registration with retired owner");
    if(!(f.module==ModuleToken{}) && t!=ACT_FUNC_SCE)throw MissingGameConsumer("ActBtn/non-scenario module callback borrowing");
    setOriginal(k,s,std::move(f),a,flags,b,t,d);
}
void ActionManager::move(){try{moveOriginal();}catch(...){init();throw;}}
void ActionManager::disp(ACT_WORK *w){if(!display)throw MissingGameConsumer("cActionButton::disp/original cMes-ID-Cockpit prompt consumer");display(*w);}
void ActionManager::resetDisplay(){if(!displayFlag)throw MissingGameConsumer("cActionButton::move/Cockpit.m_ActBttn display state");displayFlag(0);}
void ActionManager::executeScenario(ACT_WORK *w,int flag){
    auto callback=w->func;auto arg=w->arg;auto d=w->d;
    auto task=scheduler.execScenario(0x12,[callback,arg,d]{callback.invoke(arg,d);},u8(flag),w->slot,callback.taskModel,callback.module);
    if(!task)throw std::runtime_error("original action SceExec has no available task slot");
}
void ActionManager::executeArea(ACT_WORK *w){
    if(!setScenarioExec || !disableScenarioOnce)throw MissingGameConsumer("cActionButton::move/SceAt execution and one-shot owner");
    auto copy=*w;setScenarioExec(copy.arg);copy.func.invoke(copy.arg,copy.d);disableScenarioOnce(copy.arg);
}
ActionCallback PlayerDomain::actionCallback(void (PlayerDomain::*function)()){
    if(!actionLifetime || !actorModel)throw std::logic_error("original player action owner absent");
    auto weak=std::weak_ptr<const void>(actionLifetime);
    return {[this,weak,function](void*,std::intptr_t){if(weak.expired())throw std::logic_error("retired player interaction");(this->*function)();},weak,{},{}};
}
int PlayerDomain::checkXbutton(){auto *a=dynamic_cast<NativePlayerActor*>(actorModel.get());if(!a || a->player!=this)throw std::logic_error("player action virtual dispatch");return a->checkXbutton();}
int PlayerDomain::actionSelect(){int result=actionSelectOriginal();++construction.actionSelections;return result;}
void PlayerDomain::moveEye(){moveEyeOriginal();++construction.eyeMoves;}
void PlayerDomain::actorCollision(ModelCpu *p){actorCollisionOriginal(p);++construction.actorCollisionChecks;}
int PlayerDomain::weaponAimKey(){if(!Wep || !Wep->m_pWep)throw std::logic_error("original aiming weapon owner");return Wep->m_pWep->keyKamae(Key);}
u32 PlayerDomain::configurationFlags() const {if(!runtime)throw std::logic_error("player configuration owner");return runtime->configurationFlags();}
const JOY &PlayerDomain::inputJoy() const {if(!runtime)throw std::logic_error("player Joy owner");return runtime->joy(0);}
int PlayerDomain::weaponReloadable(){if(!runtime || !Wep || !Wep->m_pWep)throw std::logic_error("original weapon/inventory owner");return runtime->subscreen().weaponReloadable();}
// No SceAtInit has run in the isolated actor-construction probe. This is the
// exact original null-pS branch, not an empty *initialized* room registry.
// A bound room requires the actual shared SceAt field query before proceeding.
int PlayerDomain::windowStatus(NativeActionWindow *w){if(!w)throw std::invalid_argument("original window owner absent");return w->ChkStatus();}
ActionCallback PlayerDomain::windowAction(NativeActionWindow*){throw MissingGameConsumer("windowOn/cEmWindow::ExeWindowEvent live task owner");}
void playerActorCollision(PlayerDomain *d,ModelCpu *p){
    if(!d || !d->actorModel || p!=d->actorModel.get() || !d->EmMgr)throw std::logic_error("original Actor collision owner");
    d->actorCollision(p);
}
} // namespace re4::port
