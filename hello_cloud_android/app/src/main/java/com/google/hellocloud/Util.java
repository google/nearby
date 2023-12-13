package com.google.hellocloud;

import static androidx.core.content.PermissionChecker.PERMISSION_DENIED;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.util.Log;
import android.widget.Toast;

import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.List;

public class Util {
  static final String TAG = "HelloCloud";

  static String getDefaultDeviceName() {
    // Limit the length to 16 characters. I *think* the limit is 17.
    String name = Build.MODEL;
    if (name.length() >= 16) {
      name = name.substring(0, 15);
    }
    return name;
  }

  static void logErrorAndToast(Context context, int stringId, Object... objects) {
    String content = context.getResources().getString(stringId, objects);
    Log.e(TAG, content);
    Toast.makeText(context, content, Toast.LENGTH_SHORT).show();
  }

  /**
   * Checks whether all the permissions are granted, and requests the permissions that are not.
   *
   * @param activity    The activity that requires the permissions
   * @param permissions The permissions needed for the Activity
   * @param requestCode The code to be checked by {@link
   *                    android.app.Activity#onRequestPermissionsResult}
   * @return True means some of the permissions have not been granted and the method has requested
   * them for the Activity. False means all the permissions have already been granted.
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
}
