package com.ast.re4decomp;
import android.content.Context;
import android.content.SharedPreferences;
/** App-wide options, independent of savegames and EGL context lifetime. */
final class IngameSettings {
 private static SharedPreferences prefs(Context c){return c.getSharedPreferences("ingame-options",Context.MODE_PRIVATE);}
 static void restore(Context c){SharedPreferences s=prefs(c);
  NativeBridge.configureGraphics(s.getInt("msaa",0),s.getInt("af",8),s.getBoolean("mipmapping",true),s.getBoolean("roomFps",true),s.getBoolean("gyro",false),s.getBoolean("dof",false));
  NativeBridge.setCheatFlags(s.getInt("cheats",0)&3);
 }
 static void graphics(Context c,int msaa,int af,boolean mip,boolean room,boolean gyro,boolean dof){prefs(c).edit().putInt("msaa",msaa).putInt("af",af).putBoolean("mipmapping",mip).putBoolean("roomFps",room).putBoolean("gyro",gyro).putBoolean("dof",dof).apply();}
 static void cheats(Context c,int flags){prefs(c).edit().putInt("cheats",flags&3).apply();}
}
