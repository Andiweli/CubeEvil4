package com.ast.re4decomp;

/** Retail card.cpp file selection, confirmations and asynchronous operations.
 * Android provides one virtual card with the original twenty file slots. */
final class CardFlow {
  enum Phase { SELECT, CONFIRM, EXIT_CONFIRM, DELETE_CONFIRM, BUSY, DONE, ERROR, LEAVE, CLOSED }
  enum Operation { NONE, SAVE, LOAD, DELETE }
  final boolean saving;
  Phase phase=Phase.SELECT;
  Operation operation=Operation.NONE;
  int cursor,choice=1,message,entryFrame,scrollFrame,scrollDirection,operationFrame,exitFrame,presentationFrame;
  int entryDuration,scrollDuration;
  private OriginalSaveHeader[] headers=new OriginalSaveHeader[20];
  CardFlow(boolean saving){this.saving=saving;}
  void open(OriginalSaveHeader[] rows,int selection,int entry,int scroll) {
    refresh(rows);cursor=Math.floorMod(selection,20);entryDuration=Math.max(0,entry);scrollDuration=Math.max(0,scroll);
    phase=Phase.SELECT;message=0;choice=1;operation=Operation.NONE;entryFrame=scrollFrame=scrollDirection=operationFrame=exitFrame=presentationFrame=0;
  }
  void refresh(OriginalSaveHeader[] rows){if(rows==null||rows.length!=20)throw new IllegalArgumentException("Twenty original files required");headers=rows.clone();}
  boolean busy(){return phase==Phase.BUSY;}
  boolean selecting(){return phase==Phase.SELECT;}
  boolean choices(){return phase==Phase.CONFIRM||phase==Phase.EXIT_CONFIRM||phase==Phase.DELETE_CONFIRM;}
  // CardID hides the requester root. Its textured children inherit that
  // visibility; they must not cover the lower file rows while browsing.
  boolean unitVisible(int group,int mark){return group!=2009||message!=0||(mark!=0&&mark!=1);}
  boolean animating(){return entryFrame<entryDuration||scrollDirection!=0;}
  float animationFrame(int group,int mark,float fallback) {
    if(group!=2005&&group!=2008)return fallback;
    // Near IdSys plates have their own entry path (30 ticks). The far
    // list's path ends at14 and only that path gates CardID navigation.
    if(mark==5)return presentationFrame;
    if(group==2005&&mark==1&&(busy()||phase==Phase.DONE))return operationFrame;
    if(scrollDirection>0&&(mark==3||mark==(group==2005?4:1)))return scrollFrame;
    if(mark==1||mark==3||mark==4)return 0;
    return fallback;
  }
  void tick(){++presentationFrame;if(entryFrame<entryDuration)++entryFrame;if(scrollDirection!=0&&++scrollFrame>=scrollDuration){scrollDirection=0;scrollFrame=0;}if(busy()||phase==Phase.DONE)++operationFrame;if(phase==Phase.LEAVE&&++exitFrame>=10)phase=Phase.CLOSED;}
  private void leave(){phase=Phase.LEAVE;message=0;exitFrame=0;}
  boolean move(int direction) {
    if(direction==0||busy()||phase==Phase.LEAVE||phase==Phase.CLOSED||phase==Phase.DONE||phase==Phase.ERROR)return false;
    if(choices()){int next=direction<0?0:1;if(next==choice)return false;choice=next;return true;}
    if(animating())return false;
    cursor=Math.floorMod(cursor+Integer.signum(direction),20);scrollDirection=Integer.signum(direction);scrollFrame=0;
    if(scrollDuration==0)scrollDirection=0;return true;
  }
  Operation confirm() {
    if(busy()||phase==Phase.LEAVE||phase==Phase.CLOSED||animating())return Operation.NONE;
    if(phase==Phase.DONE){if(operation==Operation.DELETE)phase=Phase.SELECT;else leave();message=0;operation=Operation.NONE;return Operation.NONE;}
    if(phase==Phase.ERROR){phase=Phase.SELECT;message=0;return Operation.NONE;}
    if(phase==Phase.SELECT){
      OriginalSaveHeader row=headers[cursor];
      if(row!=null&&row.corrupt){phase=Phase.DELETE_CONFIRM;message=22;choice=1;return Operation.NONE;}
      if(!saving&&row==null)return Operation.NONE;
      phase=Phase.CONFIRM;message=saving?(row==null?16:19):18;choice=1;return Operation.NONE;
    }
    if(choice!=0){phase=Phase.SELECT;message=0;return Operation.NONE;}
    if(phase==Phase.EXIT_CONFIRM){leave();return Operation.NONE;}
    operation=phase==Phase.DELETE_CONFIRM?Operation.DELETE:saving?Operation.SAVE:Operation.LOAD;
    phase=Phase.BUSY;operationFrame=0;message=operation==Operation.DELETE?30:saving?11:13;return operation;
  }
  void back() {
    if(busy()||phase==Phase.LEAVE||phase==Phase.CLOSED)return;
    if(phase==Phase.DONE){confirm();return;}
    if(phase==Phase.ERROR||choices()){phase=Phase.SELECT;message=0;return;}
    if(animating())return;
    phase=Phase.EXIT_CONFIRM;message=saving?15:17;choice=0;
  }
  void completed(boolean success){
    if(!busy())return;
    phase=success?Phase.DONE:Phase.ERROR;
    message=success?(operation==Operation.DELETE?31:saving?12:14):(saving&&operation!=Operation.DELETE?1:20);
    // Retail loadMain immediately enters cCard::exit after restoring data.
    if(success&&operation==Operation.LOAD)leave();
  }
  void unsupported(){if(busy()){phase=Phase.ERROR;message=20;operation=Operation.NONE;}}
}
