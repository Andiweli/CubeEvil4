package com.ast.re4decomp;

/** Source-derived 30 Hz title task. Save/card and campaign events are separate. */
final class StartupFlow {
  enum Phase { NINTENDO, TIMELINE, MENU, NEW_GAME_FADE, OPENING, DEMO, LOAD, OPTIONS, LAUNCH }
  Phase phase = Phase.NINTENDO;
  int frame, cursor, idle, demoNumber, menuFrame,titleTicks,cursorFrame,menuFadeFrame;
  boolean logoSound, menuFadeIn;
  void tick() {
    if(phase==Phase.TIMELINE||phase==Phase.MENU||phase==Phase.NEW_GAME_FADE)++titleTicks;
    ++frame;
    switch (phase) {
      case NINTENDO: if (frame >= 78) { phase = Phase.TIMELINE; frame = 0; } break;
      case TIMELINE:
        if (frame > 345) logoSound = true;
        if (frame >= 585) { phase = Phase.MENU; frame = 585; idle = menuFrame = 0; }
        break;
      case MENU: ++menuFrame;++cursorFrame;++menuFadeFrame; if (++idle >= 600) { phase = Phase.DEMO; demoNumber ^= 1; frame = 0; } break;
      // titleExit waits SndEndCheck(se_id), independently of the 90-frame fade.
      // The audio owner supplies that completion; elapsed time cannot replace it.
      case NEW_GAME_FADE: frame=Math.min(frame,90); break;
      default: break;
    }
  }
  void skip() {
    if (phase == Phase.NINTENDO || phase == Phase.TIMELINE) {
      // Requested fast startup uses titleNintendo's direct titleMenuInit
      // destination. Begin the authored menu animation at0 exactly once.
      phase = Phase.MENU;frame = 585;idle = menuFrame = cursorFrame = menuFadeFrame = 0;
      logoSound = menuFadeIn = true;
    } else if (phase == Phase.DEMO) movieComplete();
    else if (phase == Phase.OPENING) movieComplete();
  }
  void move(int direction) { if (phase == Phase.MENU) { cursor = Math.max(0, Math.min(2, cursor + direction)); idle = cursorFrame = 0; } }
  void confirm() {
    if (phase != Phase.MENU) return;
    phase = cursor == 0 ? Phase.NEW_GAME_FADE : cursor == 1 ? Phase.LOAD : Phase.OPTIONS;
    frame = 0; idle = 0;
  }
  void back() { if (phase == Phase.LOAD || phase == Phase.OPTIONS) { phase = Phase.MENU; frame = titleFrame();cursorFrame=0;idle=0; } }
  void startGameVoiceEnded(boolean ended) {
    if (phase==Phase.NEW_GAME_FADE && frame>=90 && ended) {
      phase=Phase.OPENING;frame=0;
    }
  }
  void movieComplete() { if (phase == Phase.OPENING) phase = Phase.LAUNCH; else if (phase == Phase.DEMO) {
    // titleMain keeps the live IdSys units through Sofdec playback. Restore
    // their clocks, then fade black out for fifteen ticks; never replay or
    // freeze TITLE6's 595..645 transition at its bright intermediate frame.
    phase = Phase.MENU;frame = titleFrame();idle = menuFadeFrame = 0;menuFadeIn = true;
  } }
  float fade() {
    if (phase == Phase.NINTENDO) return Math.max(0, Math.min(1, Math.min(frame/15f, (78 - frame)/15f)));
    if (phase == Phase.NEW_GAME_FADE) return Math.max(0, 1 - frame/90f);
    return 1;
  }
  int titleFrame() { return phase==Phase.TIMELINE?Math.max(0,frame):585+Math.min(menuFrame,54000-585); }
  float menuEntryFade() { return phase==Phase.MENU&&menuFadeIn?Math.min(1,menuFadeFrame/15f):1; }
}
