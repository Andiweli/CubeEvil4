package com.ast.re4decomp;
import java.nio.file.*;
public final class CardFlowTests {
  private static int checks;
  private static void check(boolean condition,String text){++checks;if(!condition)throw new AssertionError(text);}
  private static void settle(CardFlow flow){for(int n=0;n<120;++n)flow.tick();}
  public static void main(String[]args)throws Exception {
    if(args.length!=1)throw new IllegalArgumentException("Original card fixture required");
    OriginalSaveHeader valid=new OriginalSaveHeader(Files.readAllBytes(Paths.get(args[0])));
    OriginalSaveHeader[] slots=new OriginalSaveHeader[20];slots[19]=valid;slots[3]=OriginalSaveHeader.broken();
    CardFlow load=new CardFlow(false);load.open(slots,0,10,8);
    check(!load.move(-1)&&load.confirm()==CardFlow.Operation.NONE&&load.selecting(),"Source entry animation owns navigation");settle(load);
    check(!load.unitVisible(2009,0)&&!load.unitVisible(2009,1)&&load.unitVisible(2008,1),
        "Browsing hides both original requester roots while preserving the typewriter lever");
    check(load.confirm()==CardFlow.Operation.NONE&&load.selecting(),"Empty load slot never starts a read");
    check(load.move(-1)&&load.cursor==19,"Up wraps file00 to file19");check(!load.move(-1),"Source scroll owns navigation until complete");settle(load);
    check(load.confirm()==CardFlow.Operation.NONE&&load.message==18&&load.choice==1,"Valid load requests original LOADFILE confirmation, default No");
    check(load.unitVisible(2009,0)&&load.unitVisible(2009,1),"Original requester roots become visible for LOADFILE");
    load.confirm();check(load.selecting(),"Default No retains selection and file");load.confirm();load.move(-1);
    check(load.confirm()==CardFlow.Operation.LOAD&&load.message==13,"Yes starts one original loading phase");
    load.back();check(load.busy()&&!load.move(1)&&load.confirm()==CardFlow.Operation.NONE,"Busy read cannot reenter or close");
    load.completed(false);check(load.message==20,"Read failure uses original DATALOAD failure");load.confirm();check(load.selecting()&&load.cursor==19,"Failed load leaves original file selection usable");
    load.back();check(load.message==17&&load.choice==0,"B requests original stop-loading confirmation, default Yes");load.move(1);load.confirm();check(load.selecting(),"Declining exit preserves card");load.back();load.confirm();settle(load);check(load.phase==CardFlow.Phase.CLOSED,"Confirmed exit closes once");
    CardFlow save=new CardFlow(true);save.open(slots,0,0,0);save.confirm();check(save.message==16,"New file SAVEFILE confirmation");save.move(-1);check(save.confirm()==CardFlow.Operation.SAVE&&save.message==11,"Confirmed new file enters saving");
    save.completed(false);check(save.message==1,"Write failure uses DATA_SAVE failure, not load failure");save.confirm();save.move(-1);save.confirm();check(save.message==19&&save.choice==1,"Existing file requires separate overwrite confirmation");save.back();check(save.selecting()&&save.cursor==19,"B rejects overwrite without starting I/O");
    save.back();check(save.message==15&&save.choice==0,"Save exit has SAVE_CONFIRM message");save.back();save.move(1);save.move(1);save.move(1);save.move(1);check(save.cursor==3,"Select actual corrupt row");save.confirm();check(save.message==22&&save.choice==1,"Corrupt row offers original delete confirmation, default No");save.back();check(save.selecting(),"Corrupt file never deleted by merely inspecting it");save.confirm();save.move(-1);check(save.confirm()==CardFlow.Operation.DELETE&&save.message==30,"Confirmed delete has explicit asynchronous deleting phase");
    save.completed(true);check(save.message==31,"Delete completion distinct from save completion");save.confirm();check(save.selecting(),"Acknowledged deletion returns to list");
    save.move(-1);save.confirm();save.move(-1);save.confirm();save.completed(true);check(save.message==12,"Save completion original message");save.confirm();check(save.phase==CardFlow.Phase.LEAVE,"Save acknowledgement starts original ten-frame exit fade");settle(save);check(save.phase==CardFlow.Phase.CLOSED,"Acknowledged save resumes game");
    CardFlow exitSave=new CardFlow(true);exitSave.open(slots,0,0,0);exitSave.back();check(exitSave.choice==0,"Typewriter exit defaults to Yes");exitSave.confirm();settle(exitSave);check(exitSave.phase==CardFlow.Phase.CLOSED,"Default save exit resumes game without writing");
    CardFlow loaded=new CardFlow(false);loaded.open(slots,19,0,0);loaded.confirm();loaded.move(-1);loaded.confirm();loaded.completed(true);
    check(loaded.phase==CardFlow.Phase.LEAVE,"Successful load immediately follows original exit fade without extra confirmation");
    for(int n=0;n<9;++n)loaded.tick();check(loaded.phase==CardFlow.Phase.LEAVE&&!loaded.move(1),"Original exit fade owns nine preceding ticks and input");loaded.tick();check(loaded.phase==CardFlow.Phase.CLOSED,"Loaded card closes after exactly ten source ticks");
    CardFlow animated=new CardFlow(true);animated.open(slots,0,0,0);settle(animated);
    check(animated.presentationFrame==120 && animated.operationFrame==0,"Typewriter texture clock runs before saving");
    animated.confirm();animated.move(-1);animated.confirm();animated.tick();
    check(animated.presentationFrame==121 && animated.operationFrame==1,"Writing restarts its motion while texture animation stays continuous");
    CardFlow entry=new CardFlow(false);entry.open(slots,0,14,4);
    for(int n=0;n<14;++n)entry.tick();
    check(!entry.animating()&&entry.entryFrame==14,"Original far list finishes its fourteen-frame path");
    for(int n=0;n<16;++n)entry.tick();
    check(entry.entryFrame==14&&entry.animationFrame(2008,5,90)==30,
        "Original LOAD plate reaches its own thirty-frame endpoint after the list stops");
    check(entry.animationFrame(2005,5,90)==30&&entry.animationFrame(2008,1,90)==0,
        "SAVE plate has the same independent entry clock; idle typewriter lever stays at rest");
    entry.move(1);entry.tick();
    check(entry.animationFrame(2008,5,90)==31&&entry.animationFrame(2008,1,90)==1&&entry.animationFrame(2008,3,90)==1,
        "Down scroll moves both original LOAD levers without restarting the near entry plate");
    settle(entry);entry.phase=CardFlow.Phase.BUSY;entry.operationFrame=3;
    check(entry.animationFrame(2008,1,90)==0&&entry.animationFrame(2005,1,90)==3,
        "Only saving starts the original writing path; loading retains the typewriter lever at rest");
    System.out.println("Original Card flow: "+checks+" checks PASS");
  }
}
