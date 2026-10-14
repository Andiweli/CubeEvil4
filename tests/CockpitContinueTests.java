package com.ast.re4decomp;

/** Source thresholds/timers, native-clock interpolation and input gates. */
public final class CockpitContinueTests {
  private static int checks;
  private static void check(boolean value){++checks;if(!value)throw new AssertionError("HUD/Continue check "+checks);}
  private static void near(float a,float b){check(Math.abs(a-b)<.002f);}
  private static void ticks(ContinueFlow flow,int count){for(int n=0;n<count;++n)flow.tick();}
  public static void main(String[] args) {
    check(CockpitState.lifeLevel(801)==0);check(CockpitState.lifeLevel(800)==1);
    check(CockpitState.lifeLevel(401)==1);check(CockpitState.lifeLevel(400)==2);
    near(CockpitState.maximumRotation(1200),-45);near(CockpitState.maximumRotation(2400),-180);
    CockpitState meter=new CockpitState();int[] state={1200,1200,10,20,35,-1,0,0,1};
    float[] color={255,200,100,255};meter.update(state,color,color);near(meter.life,1200);
    state[0]=400;state[7]=1;meter.update(state,color,color);near(meter.life,1120);
    for(int frame=0;frame<120;++frame)meter.update(state,color,color);near(meter.life,1120);
    state[7]=3;meter.update(state,color,color);near(meter.life,983.2f);
    state[8]=2;state[7]=0;meter.update(state,color,color);near(meter.life,400);
    meter.life=800;check(!meter.segmentVisible(8));near(meter.rotation(7),90);
    meter.life=1600;check(!meter.segmentVisible(9));near(meter.rotation(8),0);
    meter.life=2400;near(meter.rotation(9),-90);
    int[] partner={1200,1200,10,20,35,-1,0,0,3,0,600,600,0,0};
    check(!CockpitState.ashleyPresent(partner));partner[9]=2;check(!CockpitState.ashleyPresent(partner));
    partner[9]=3;check(CockpitState.ashleyPresent(partner));
    check(CockpitState.partnerLevel(451,600)==0 && CockpitState.partnerLevel(450,600)==1);
    check(CockpitState.partnerLevel(151,600)==1 && CockpitState.partnerLevel(150,600)==2);
    near(CockpitState.partnerMaximumRotation(600),0);near(CockpitState.partnerMaximumRotation(1200),90);
    meter.update(partner,color,color,color,color);near(meter.partnerLife,600);near(meter.partnerRotation(4),178);check(!meter.partnerSegmentVisible(5));
    partner[10]=200;partner[7]=1;meter.update(partner,color,color,color,color);near(meter.partnerLife,560);
    for(int frame=0;frame<120;++frame)meter.update(partner,color,color,color,color);near(meter.partnerLife,560);
    meter.partnerLife=1200;check(meter.partnerSegmentVisible(5));near(meter.partnerRotation(4),180);near(meter.partnerRotation(5),-90);
    check(CockpitState.partnerIconVisible(0x1c,8|1|0x10) && !CockpitState.partnerIconVisible(0x1d,8|0x10));
    check(CockpitState.partnerIconVisible(0x1e,1|2) && !CockpitState.partnerIconVisible(0x1a,1|2));
    check(CockpitState.partnerIconVisible(0x1a,2) && CockpitState.partnerIconVisible(0x1d,0x10) && CockpitState.partnerIconVisible(0x1b,0x24));
    ContinueFlow flow=new ContinueFlow();ticks(flow,269);check(!flow.choicesVisible());
    flow.tick();check(flow.phase==ContinueFlow.Phase.GATE&&flow.yes&&flow.choiceFrame==0);
    check(flow.confirm()==-1);ticks(flow,14);check(flow.phase==ContinueFlow.Phase.GATE);
    flow.tick();check(flow.phase==ContinueFlow.Phase.CHOICE);
    check(flow.move(false)==6&&!flow.yes&&flow.cursorFrame==0);check(flow.move(false)==-1);
    check(flow.move(true)==6&&flow.yes);check(flow.confirm()==8&&flow.remaining==177);
    check(flow.move(false)==-1&&flow.yes);ticks(flow,176);check(flow.phase==ContinueFlow.Phase.EXIT);
    flow.tick();check(flow.phase==ContinueFlow.Phase.DONE);
    flow=new ContinueFlow();check(flow.confirm()==-1&&flow.phase==ContinueFlow.Phase.GATE);
    ticks(flow,15);check(flow.move(false)==6);check(flow.confirm()==5&&flow.remaining==150);
    ticks(flow,149);check(flow.phase==ContinueFlow.Phase.EXIT);flow.tick();check(flow.phase==ContinueFlow.Phase.DONE&&!flow.yes);
    System.out.println("Original HUD thresholds/30-Hz clock and Continue timers/choices: "+checks+" checks PASS");
  }
}
