package com.ast.re4decomp;

/** Frozen game.cpp::gameDiedemo after DiedemoExec's90 world ticks. */
final class ContinueFlow {
  enum Phase { DEAD, GATE, CHOICE, EXIT, DONE }
  Phase phase=Phase.DEAD;
  boolean yes=true;
  int deadFrame,choiceFrame,cursorFrame,exitFrame,gate,remaining;
  void tick() {
    ++deadFrame;
    if(phase!=Phase.DEAD)++choiceFrame;
    ++cursorFrame;
    switch(phase) {
      case DEAD: if(deadFrame>=270)open();break;
      case GATE: if(++gate>=15)phase=Phase.CHOICE;break;
      case EXIT: ++exitFrame;if(--remaining<=0)phase=Phase.DONE;break;
      default: break;
    }
  }
  private void open(){phase=Phase.GATE;gate=choiceFrame=cursorFrame=0;}
  int confirm() {
    if(phase==Phase.DEAD){open();return -1;}
    if(phase!=Phase.CHOICE)return -1;
    phase=Phase.EXIT;exitFrame=0;remaining=yes?177:150;return yes?8:5;
  }
  int move(boolean selectYes) {
    if(phase!=Phase.CHOICE || yes==selectYes)return -1;
    yes=selectYes;cursorFrame=0;return 6;
  }
  boolean choicesVisible(){return phase!=Phase.DEAD;}
  boolean leaving(){return phase==Phase.EXIT || phase==Phase.DONE;}
}
