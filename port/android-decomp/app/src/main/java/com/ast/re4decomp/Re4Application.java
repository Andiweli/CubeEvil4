package com.ast.re4decomp;
import android.app.Application;
public final class Re4Application extends Application {
  public ImportManager imports;
  @Override
  public void onCreate() {
    super.onCreate();
    imports = new ImportManager(this);
  }
}
