package com.ast.re4decomp;
public final class StartupOptionsTests {
  static int count;static void check(boolean ok){++count;if(!ok)throw new AssertionError("OptionScreen "+count);}
  public static void main(String[]args){StartupOptions o=new StartupOptions();o.open(false,true,false,64,0);
    o.move(-1);check(o.top==1);check(o.group()==1010&&o.message()==0x82);o.confirm();check(o.page==StartupOptions.Page.CONTROLLER&&o.row==0);
    o.adjust(-1);check(o.pendingReverse&&!o.reverse);o.confirm();check(o.reverse);o.move(2);o.adjust(1);check(o.pendingKnife&&!o.knife);o.confirm();check(o.knife);
    check(!o.back()&&o.row==3);check(!o.back()&&o.page==StartupOptions.Page.TOP);o.move(1);o.confirm();check(o.page==StartupOptions.Page.BRIGHTNESS);
    for(int i=0;i<100;++i)o.adjust(-1);check(o.brightness==34);for(int i=0;i<100;++i)o.adjust(1);check(o.brightness==114);o.back();o.confirm();o.move(1);o.confirm();check(o.page==StartupOptions.Page.AUDIO&&o.row==1);
    o.move(-1);o.confirm();check(o.soundMode==1);o.move(2);o.confirm();check(o.soundMode==2);o.back();o.back();o.back();check(o.top==4);check(o.confirm());
    System.out.println("Startup original options: "+count+" checks PASS");}
}
