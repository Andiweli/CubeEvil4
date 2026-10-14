package com.ast.re4decomp;
public final class NativeBridge {
  static native String configureGraphics(int msaa,int af,boolean mipmapping,boolean roomFps,boolean gyro,boolean dof);
  public static native void gyroAvailable(boolean available);
  public static native void gyroRate(float yaw,float pitch);
  static native boolean roomFpsVisible();
  static native int[] graphicsOptions();
  static { System.loadLibrary("re4decomp"); }
  private NativeBridge() {}
  public static native void resetInput();
  public static native void input(int buttons, float lx, float ly, float rx,
                                  float ry, float lt, float rt);
  public static native void create(String tplPath, int imageIndex);
  public static native void environmentData(String corePath);
  public static native void configurePlayMode(boolean explore, long session);
  public static native void configureCampaignMode(boolean enabled, int difficulty, long session);
  public static native void configureOpeningCanceled(boolean canceled);
  public static native boolean saveRequested();
  public static native boolean deathRequested();
  public static native int[] deathBackdrop();
  public static native boolean continueGame();
  public static native void continueSound(int cue);
  public static native void continueConfirmed();
  public static native byte[] saveGame(int slot, int[] calendar);
  public static native long saveWriteId();
  public static native boolean completeSaveWrite(long transaction, boolean written);
  public static native void finishSave();
  public static native String savedGameProblem(byte[] image);
  public static native void configureSavedGame(byte[] image);
  public static native void configureAimInversion(boolean inverted);
  public static native void configureDisplayBrightness(int value);
  public static native void configureTestProfile(int profile, long session);
  public static native void configureWeapons(String handgunDrs, String shotgunDrs);
  public static native void configureMusic(String musicDirectory);
  public static native void configureSubscreen(String discFilesDirectory, int language);
  public static native void setSubscreenOpen(boolean open);
  public static native void subscreenItem(int id, boolean examine);
  public static native void setSubscreenModelViewport(float left, float top,
                                                     float width, float height);
  public static native void startupConfigure(String discFilesDirectory, int language);
  public static native String startupLayout(int group);
  public static native int[] startupTexture(int textureId, int frame);
  public static native void startupCompose(int[] framebuffer,int[] texture,int[] mask,float[] parameters);
  public static native void startupCue(int block, int cue);
  public static native boolean startupVoiceEnded();
  public static native void startupAudio(boolean active);
  public static native boolean eventActive();
  public static native boolean messageActive();
  public static native String subscreenSnapshot();
  public static native String subscreenLayout(String pack);
  public static native int[] subscreenTexture(String pack, int id, int frame);
  public static native void createScene(String decodedRoomPath,
                                        String playerDrs, String weaponDrs,
                                        String roomInfo, String roomId,
                                        long session);
  public static native String pendingRoomRequest();
  public static native void finishRoomRequest(long token, String decodedPath,
                                              String error);
  public static native void requestDoorTest();
  public static native void configureAmbient(String chickenDrs, String chickenList);
  public static native void configureEnemy(String enemyDrs, String enemyList);
  public static native void requestGanadoTest();
  public static native int cheatFlags();
  public static native void setCheatFlags(int flags);
  public static native String giveTestWeapon(int id);
  public static native void requestVillageTest();
  public static native void requestInspector();
  public static native void resize(int width, int height);
  public static native void draw();
  public static native int renderAudio(short[] stereoPcm);
  public static native void audioActive(boolean active);
  public static native void subscreenAudio(boolean active);
  public static native void menuSound(int cue);
  public static native String status();
  public static native String hud();
  public static native int[] cockpitState();
  public static native int[] binocularState();
  public static native int[] scopeState();
  public static native int[] eventTitleState();
  public static native String roomId();
  public static native String inventory();
  public static native String caseSnapshot();
  public static native void caseAction(int operation, int slot, int first, int second, int third);
  public static native String pendingPickup();
  public static native void answerPickup(long token, boolean accepted);
  public static native void answerPickupChoice(long token, int choice);
  public static native void requestUseGreenHerb();
  public static native String inspectArchive(String decodedPath,
                                             String previewPath, int imageIndex)
      throws java.io.IOException;
  public static native String inspectScene(String decodedPath)
      throws java.io.IOException;
  public static native String decodeRoom(String sourcePath, String outputPath)
      throws java.io.IOException;
}
