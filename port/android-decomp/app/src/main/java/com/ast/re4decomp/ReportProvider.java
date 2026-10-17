package com.ast.re4decomp;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import java.io.File;
import java.io.FileNotFoundException;

/**
 * Hands the error report (CoreLog, a zip in the cache) to the app the tester
 * sends it with (mail, messenger, cloud storage): read only, one file by
 * name, for the apps the share intent grants it to. A plain provider of its
 * own, as the app has no support libraries.
 */
public final class ReportProvider extends ContentProvider {
  static File folder(Context c) {
    File dir = new File(c.getCacheDir(), "bericht");
    if (!dir.isDirectory()) dir.mkdirs();
    return dir;
  }

  static Uri uri(Context c, File file) {
    return new Uri.Builder().scheme("content").authority(c.getPackageName() + ".report")
        .appendPath(file.getName()).build();
  }

  private File file(Uri uri) throws FileNotFoundException {
    String name = uri.getLastPathSegment();
    if (name == null || name.contains("/") || name.startsWith(".")) throw new FileNotFoundException(String.valueOf(uri));
    File f = new File(folder(getContext()), name);
    if (!f.isFile()) throw new FileNotFoundException(String.valueOf(uri));
    return f;
  }

  @Override public boolean onCreate() { return true; }

  @Override public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
    return ParcelFileDescriptor.open(file(uri), ParcelFileDescriptor.MODE_READ_ONLY);
  }

  @Override public String getType(Uri uri) { return "application/zip"; }

  @Override public Cursor query(Uri uri, String[] projection, String selection, String[] args, String order) {
    File f;
    try {
      f = file(uri);
    } catch (FileNotFoundException e) {
      return null;
    }
    String[] columns = projection != null ? projection : new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
    MatrixCursor cursor = new MatrixCursor(columns);
    Object[] row = new Object[columns.length];
    for (int i = 0; i < columns.length; i++) {
      if (OpenableColumns.DISPLAY_NAME.equals(columns[i])) row[i] = f.getName();
      else if (OpenableColumns.SIZE.equals(columns[i])) row[i] = f.length();
    }
    cursor.addRow(row);
    return cursor;
  }

  @Override public Uri insert(Uri uri, ContentValues values) { return null; }
  @Override public int delete(Uri uri, String selection, String[] args) { return 0; }
  @Override public int update(Uri uri, ContentValues values, String selection, String[] args) { return 0; }
}
