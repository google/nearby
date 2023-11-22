package com.google.location.nearby.apps.helloconnections;

import static androidx.core.content.PermissionChecker.PERMISSION_DENIED;

import android.app.Activity;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.ContactsContract;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;
import androidx.annotation.Nullable;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import com.google.common.io.ByteStreams;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/** Util class for Nearby Connections */
class Util {
  static final String TAG = "HelloConnections";

  static String getDefaultDeviceName(Context context) {
    String firstName = "";
    Cursor c =
        context
            .getContentResolver()
            .query(ContactsContract.Profile.CONTENT_URI, null, null, null, null);
    int count = c.getCount();
    String[] columnNames = c.getColumnNames();
    c.moveToFirst();
    int position = c.getPosition();
    if (count == 1 && position == 0) {
      for (String columnName : columnNames) {
        String columnValue = c.getString(c.getColumnIndex(columnName));
        if ("display_name".equals(columnName)) {
          firstName = columnValue;
        }
      }
    }
    c.close();

    if (firstName == null || firstName.isEmpty()) {
      firstName = "Unknown";
    }

    return firstName + "'s " + Build.MODEL;
  }

  static void logErrorAndToast(Context context, int stringId, Object... objects) {
    String content = context.getResources().getString(stringId, objects);
    Log.e(TAG, content);
    Toast.makeText(context, content, Toast.LENGTH_SHORT).show();
  }

  /**
   * Checks whether all the permissions are granted, and requests the permissions that are not.
   *
   * @param activity The activity that requires the permissions
   * @param permissions The permissions needed for the Activity
   * @param requestCode The code to be checked by {@link
   *     android.app.Activity#onRequestPermissionsResult}
   * @return True means some of the permissions have not been granted and the method has requested
   *     them for the Activity. False means all the permissions have already been granted.
   */
  static boolean requestPermissionsIfNeeded(
      Activity activity, String[] permissions, int requestCode) {
    List<String> permissionsToBeRequested = new ArrayList<>();
    for (String permission : permissions) {
      if (PERMISSION_DENIED == ContextCompat.checkSelfPermission(activity, permission)) {
        permissionsToBeRequested.add(permission);
      }
    }
    if (!permissionsToBeRequested.isEmpty()) {
      ActivityCompat.requestPermissions(
          activity, permissionsToBeRequested.toArray(new String[0]), requestCode);
      return true;
    } else {
      return false;
    }
  }

  static byte[] generateRandomBytes(int size) {
    byte[] bytes = new byte[size];
    new Random().nextBytes(bytes);

    return bytes;
  }

  /**
   * Copies the file to local directory with the {@link Uri} and delete the source file.
   *
   * @param uri the {@link Uri} to read from
   */
  static void copyFileFromUri(Context context, Uri uri) {
    try {
      InputStream inputStream = context.getContentResolver().openInputStream(uri);
      copyStream(
          inputStream,
          new FileOutputStream(new File(context.getCacheDir(), uri.getLastPathSegment())));
    } catch (IOException e) {
      Log.e(TAG, "Copy file to local directory but failed.", e);
    } finally {
      // Delete the original file.
      context.getContentResolver().delete(uri, null, null);
    }
  }

  /** Copies a stream from one location to another. */
  private static void copyStream(InputStream in, OutputStream out) throws IOException {
    try {
      ByteStreams.copy(in, out);
      out.flush();
    } finally {
      in.close();
      out.close();
    }
  }

  /**
   * Convert a Bluetooth MAC address from string to byte array format. Returns null if input string
   * is not of correct format.
   *
   * <p>e.g. "AC:37:43:BC:A9:28" -> {-84, 55, 67, -68, -87, 40}.
   */
  @Nullable
  public static byte[] macAddressToBytes(@Nullable String macAddress) {
    if (TextUtils.isEmpty(macAddress)) {
      return null;
    }
    byte[] bytes = new byte[6];
    String[] address = macAddress.split(":");
    if (address.length != 6) {
      return null;
    }
    for (int i = 0; i < 6; i++) {
      bytes[i] = Integer.decode("0x" + address[i]).byteValue();
    }
    return bytes;
  }
}
