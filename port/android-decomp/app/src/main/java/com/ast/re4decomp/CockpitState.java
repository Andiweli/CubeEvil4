package com.ast.re4decomp;

/** Frozen GC cockpit.cpp::LifeMeter and pl_class.cpp::getLifeLevel rules. */
final class CockpitState {
  float life, partnerLife;
  int level, tick=-1, epoch=-1;
  final float[] color0=new float[4],color1=new float[4];
  final float[] partnerColor0=new float[4],partnerColor1=new float[4];
  void update(int[] state,float[] target0,float[] target1) {
    update(state,target0,target1,target0,target1);
  }
  void update(int[] state,float[] target0,float[] target1,float[] sub0,float[] sub1) {
    if(state==null || state.length<9)return;
    int subLife=state.length>=14?state[10]:600;
    int steps;
    if(epoch!=state[8] || tick<0 || state[7]<tick) {
      epoch=state[8];life=state[0];partnerLife=subLife;steps=1;
      java.util.Arrays.fill(color0,0);java.util.Arrays.fill(color1,0);
      java.util.Arrays.fill(partnerColor0,0);java.util.Arrays.fill(partnerColor1,0);
    } else steps=state[7]-tick;
    tick=state[7];level=lifeLevel(state[0]);
    for(int n=0;n<steps;++n) {
      life=.9f*life+.1f*state[0];
      partnerLife=.9f*partnerLife+.1f*subLife;
      for(int k=0;k<4;++k){color0[k]=.9f*color0[k]+.1f*target0[k];color1[k]=.9f*color1[k]+.1f*target1[k];}
      for(int k=0;k<4;++k){partnerColor0[k]=.9f*partnerColor0[k]+.1f*sub0[k];partnerColor1[k]=.9f*partnerColor1[k]+.1f*sub1[k];}
    }
  }
  static int lifeLevel(int life){return life>800?0:life>400?1:2;}
  static float maximumRotation(int max){int upgrades=(int)(20f*(max-1200)/1200+.5f);return upgrades*-135f/20-45;}
  static boolean ashleyPresent(int[] state){return state!=null && state.length>=14 && state[9]==3;}
  static int partnerLevel(int life,int max){return life>max*3/4?0:life>max/4?1:2;}
  static float partnerMaximumRotation(int max){return (int)(5f*(max-600)/600+.5f)*90f/5;}
  float partnerRotation(int mark){float rate=partnerLife/200;return mark==4?(rate>3?180:rate*30+88):(rate-3)*30-180;}
  boolean partnerSegmentVisible(int mark){return mark==4?partnerLife>=0:partnerLife>600;}
  static boolean partnerIconVisible(int mark,int condition) {
    if((condition&8)!=0)return mark==0x1c;
    if(mark==0x1e)return (condition&1)!=0;
    if(mark==0x1a)return (condition&1)==0 && (condition&2)!=0;
    if(mark==0x1d)return (condition&0x10)!=0;
    return mark==0x1b && (condition&0x24)!=0;
  }
  float rotation(int mark) {
    float rate=life/400;
    if(mark==7)return rate>2?90:180-rate*45;
    if(mark==8)return rate>4?0:90-(rate-2)*45;
    return -(rate-4)*45;
  }
  boolean segmentVisible(int mark){return mark==7?life>=0:mark==8?life>800:life>1600;}
}
