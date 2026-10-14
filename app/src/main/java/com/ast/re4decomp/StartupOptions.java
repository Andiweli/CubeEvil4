package com.ast.re4decomp;

/** OptionScreen's original title cursor, staged controller values and bounds. */
final class StartupOptions {
  enum Page { TOP, CONTROLLER, BRIGHTNESS, AUDIO }
  Page page=Page.TOP;
  int top=1,row,brightness=64,soundMode=1;
  boolean reverse,vibration=true,knife,pendingReverse,pendingVibration,pendingKnife;
  void open(boolean reverse,boolean vibration,boolean knife,int brightness,int soundMode) {
    this.reverse=reverse;this.vibration=vibration;this.knife=knife;
    this.brightness=Math.max(34,Math.min(114,brightness));this.soundMode=Math.max(0,Math.min(2,soundMode));
    top=1;row=0;page=Page.TOP;
  }
  boolean confirm() {
    if(page==Page.TOP){if(top==4)return true;page=top==1?Page.CONTROLLER:top==2?Page.BRIGHTNESS:Page.AUDIO;row=page==Page.AUDIO?(soundMode==0?1:soundMode==1?0:2):0;
      pendingReverse=reverse;pendingVibration=vibration;pendingKnife=knife;return false;}
    if(row==backRow()){page=Page.TOP;return false;}
    if(page==Page.CONTROLLER){if(row==0)reverse=pendingReverse;else if(row==1)vibration=pendingVibration;else knife=pendingKnife;}
    if(page==Page.AUDIO)soundMode=row==0?1:row==1?0:2;
    return false;
  }
  boolean back(){if(page==Page.TOP){if(top==4)return true;top=4;}else if(row==backRow())page=Page.TOP;else row=backRow();return false;}
  void move(int direction){if(page==Page.TOP)top=Math.max(1,Math.min(4,top+direction));else row=Math.max(0,Math.min(backRow(),row+direction));}
  void adjust(int direction){if(page==Page.CONTROLLER){if(row==0)pendingReverse=direction<0;else if(row==1)pendingVibration=direction<0;else if(row==2)pendingKnife=direction>0;}
    else if(page==Page.BRIGHTNESS&&row==0)brightness=Math.max(34,Math.min(114,brightness+direction));}
  int backRow(){return page==Page.BRIGHTNESS?1:3;}
  int group(){return page==Page.TOP?1010:page==Page.CONTROLLER?1012:page==Page.BRIGHTNESS?1013:1014;}
  int message(){if(page==Page.TOP)return 0x81+top;if(page==Page.CONTROLLER)return new int[]{0x8b,0x8c,0x92,0x86}[row];if(page==Page.BRIGHTNESS)return row==0?0x8d:0x86;return row==3?0x86:0x8e;}
}
