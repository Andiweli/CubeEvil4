package com.ast.re4decomp;

/** SndEndCheck and the separate Android output queue must both finish. */
public final class StartupVoiceTests {
  private static int checks;
  private static void check(boolean pass,String message) {
    ++checks;if(!pass)throw new AssertionError(message);
  }
  private static StartupFlow newGame() {
    StartupFlow f=new StartupFlow();f.phase=StartupFlow.Phase.MENU;f.cursor=0;f.confirm();return f;
  }
  public static void main(String[]args) {
    StartupFlow early=newGame();
    for(int i=0;i<89;++i){early.tick();early.startGameVoiceEnded(true);}
    check(early.phase==StartupFlow.Phase.NEW_GAME_FADE,"Voice end bypassed source 90-frame fade");
    early.tick();early.startGameVoiceEnded(true);
    check(early.phase==StartupFlow.Phase.OPENING,"Finished cue/fade did not start movie");
    StartupFlow late=newGame();
    for(int i=0;i<9000;++i){late.tick();late.startGameVoiceEnded(false);}
    check(late.phase==StartupFlow.Phase.NEW_GAME_FADE&&late.frame==90&&late.fade()==0,
        "Timed fade truncated voice or invented a completion timeout");
    late.skip();late.confirm();late.move(1);
    check(late.phase==StartupFlow.Phase.NEW_GAME_FADE&&late.cursor==0,
        "Navigation/skip interrupted protected title voice");
    AudioPlaybackFence output=new AudioPlaybackFence();
    output.wrote(1024);output.wrote(1024);output.wrote(1024);
    long tail=output.queuedFrame();
    check(tail==1536,"Stereo frame fence is in sample units");
    late.startGameVoiceEnded(output.playedFrame(1024)>=tail);
    check(late.phase==StartupFlow.Phase.NEW_GAME_FADE,"Decoded cue end flushed queued speech tail");
    late.startGameVoiceEnded(output.playedFrame(1535)>=tail);
    check(late.phase==StartupFlow.Phase.NEW_GAME_FADE,"Last written PCM frame was truncated");
    check(output.playedFrame(1535)==1535,"Paused output advanced playback position");
    late.startGameVoiceEnded(output.playedFrame(1536)>=tail);
    check(late.phase==StartupFlow.Phase.OPENING,"Playback-head drain did not release movie");
    output.reset();output.wrote(511);output.wrote(513);
    check(output.queuedFrame()==512,"Partial AudioTrack writes lost stereo sample accounting");
    check(output.playedFrame(-2)==4294967294L,"Unsigned AudioTrack position was signed");
    check(output.playedFrame(3)==4294967299L,"32-bit playback-head wrap lost its epoch");
    output.reset();check(output.queuedFrame()==0&&output.playedFrame(0)==0,
        "AudioTrack flush retained stale playback frame epoch");
    late.movieComplete();late.startGameVoiceEnded(false);
    check(late.phase==StartupFlow.Phase.LAUNCH,"Later title cue changed completed launch");
    System.out.println("Startup original voice end/output drain: "+checks+" checks PASS");
  }
}
