package com.ast.re4decomp;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.RectF;
import android.view.KeyEvent;

/** sceAtGetItem: source model camera and CORE question/name/number/choices. */
final class PickupDialog extends GameScreenDialog {
  interface Answer { void onAnswer(int choice); }
  private final int itemId, message, choices, count;
  private boolean fits, answered;
  private int selection;
  private final Answer answer;
  private final Runnable orderCase;
  private final GameMenuArt art = new GameMenuArt(this::refreshScreen);
  PickupDialog(Activity activity,int itemId,long count,boolean fits,int message,int choices,
               Answer answer,Runnable orderCase) {
    super(activity);this.itemId=itemId;this.count=(int)Math.max(0,Math.min(99999999,count));
    this.fits=fits;this.message=message;this.choices=choices;this.answer=answer;this.orderCase=orderCase;
  }
  void setFits(boolean available) { fits=available;refreshScreen(); }
  @Override public void dismiss() { art.close();super.dismiss(); }
  @Override float sourceHeight() {return GameMenuLayout.PICKUP_HEIGHT;}
  @Override void drawScreen(Canvas c) {
    art.viewport(layout);
    // ItemExamine fills the screen below Message::trans. Inventory chrome
    // and Android buttons do not belong to the original item-get requester.
    modelWindow(c,new RectF(0,0,layout.width,layout.height),false);
    art.pickupMessage(c,paint,itemId,message,count,selection);
    if(choices==0)hotspot(new RectF(0,0,layout.width,layout.height),this::confirm);
    else for(int n=0;n<choices;++n) {
      final int choice=n;
      hotspot(new RectF(layout.centerX()-180+n*360f/choices,
          layout.extraY*.5f+330-GameMenuLayout.PICKUP_TOP,layout.centerX()-180+(n+1)*360f/choices,
          layout.extraY*.5f+377-GameMenuLayout.PICKUP_TOP),()->{selection=choice;confirm();});
    }
  }
  @Override void direction(int x,int y) {
    if(answered || choices==0)return;
    int step=x!=0?x:y;selection=Math.floorMod(selection+Integer.signum(step),choices);
    NativeBridge.menuSound(0xE);refreshScreen();
  }
  @Override void confirm() {
    if(answered)return;
    if(choices>0 && selection==0 && !fits){orderCase.run();return;}
    answered=true;answer.onAnswer(choices==0?1:selection+1);dismiss();
  }
  @Override void back() {
    if(answered)return;
    answered=true;answer.onAnswer(choices==0?1:2);dismiss();
  }
  @Override void auxiliary(int key) {
    if(!answered && choices>0 && !fits && key==KeyEvent.KEYCODE_BUTTON_Y)orderCase.run();
  }
  void abandon() { if(!answered){answered=true;answer.onAnswer(choices==0?1:2);}dismiss(); }
}
