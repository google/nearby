package com.google.hellocloud;

import static androidx.core.content.PermissionChecker.PERMISSION_DENIED;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore;
import android.util.Log;
import android.widget.Toast;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class Utils {
  static final String TAG = "HelloCloud";

  static String getDefaultDeviceName() {
    return "Artoo";
    // Limit the length to 16 characters. I *think* the limit is 17.
    //    String name = Build.MODEL;
    //    if (name.length() >= 16) {
    //      name = name.substring(0, 15);
    //    }
    //    return name;
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

  public static Packet<IncomingFile> createIncomingDebugModel() {
    Packet<IncomingFile> packet = new Packet<>();
    packet.id = UUID.fromString("117442B8-CD26-4E13-B233-3678C339BDBD");
    packet.sender = "Princess Leia";
    packet.setState(Packet.State.RECEIVED);

    var file1 =
        new IncomingFile("image/jpeg")
            .setFileSize(1024 * 1024 * 8)
            .setState(IncomingFile.State.DOWNLOADED)
            .setLocalUri(Uri.parse("content://media/external_primary/images/media/1000000491"));
    file1.remotePath = "12A7368E-9BC6-4C49-B485-FC1E6B9F63EC.png";
    //    var file2 =
    //        new IncomingFile("image/jpeg")
    //            .setFileSize(1024 * 1024 * 128)
    //            .setState(IncomingFile.State.DOWNLOADING);
    packet.files.add(file1);
    //    packet.files.add(file2);
    return packet;
  }

  public static Packet<OutgoingFile> createOutgoingDebugModel() {
    Packet<OutgoingFile> packet = new Packet<>();
    packet.notificationToken =
        "dUcjcnLNZ0hxuqWScq2UDh:APA91bGG8GTykBZgAkGA_xkBVnefjUb-PvR4mDNjwjv1Sv7EYGZc89zyfoy6Syz63cQ3OkQUH3D5Drf0674CZOumgBsgX8sR4JGQANWeFNjC_RScHWDyA8ZhYdzHdp7t6uQjqEhF_TEL";
    packet.receiver = "Luke Skywalker";
    packet.setState(Packet.State.LOADED);

    var file1 =
        new OutgoingFile("image/jpeg")
            .setFileSize(1024 * 1024 * 4)
            .setState(OutgoingFile.State.UPLOADED);
    var file2 =
        new OutgoingFile("image/jpeg")
            .setFileSize(1024 * 1024 * 4)
            .setState(OutgoingFile.State.LOADED);

    packet.files.add(file1);
    packet.files.add(file2);
    return packet;
  }

  public static Main createDebugMain() {
    Main main = new Main();

    if (!Config.debugEndpoint) {
      return main;
    }

    Endpoint endpoint = new Endpoint("R2D2", "Debug droid", false);
    endpoint.setState(Endpoint.State.CONNECTED);
    main.addEndpoint(endpoint);
    main.addIncomingPacket(createIncomingDebugModel());
    main.addOutgoingPacket(createOutgoingDebugModel());
    return main;
  }

  public static Packet<OutgoingFile> loadPhotos(
      Context context, List<Uri> uris, String receiver, String notificationToken) {
    Packet<OutgoingFile> packet = new Packet<>();
    packet.notificationToken = notificationToken;
    packet.setState(Packet.State.LOADED);
    packet.receiver = receiver;
    packet.sender = Main.shared.getLocalEndpointName();

    ContentResolver resolver = context.getContentResolver();

    for (Uri uri : uris) {
      String mimeType = resolver.getType(uri);

      // Get the file size.
      Cursor cursor =
          resolver.query(uri, new String[] {MediaStore.MediaColumns.SIZE}, null, null, null);

      assert cursor != null;
      int sizeIndex = cursor.getColumnIndex(MediaStore.MediaColumns.SIZE);
      cursor.moveToFirst();

      int size = cursor.getInt(sizeIndex);
      cursor.close();

      // Construct a file to be added to the packet
      OutgoingFile file =
          new OutgoingFile(mimeType)
              .setState(OutgoingFile.State.LOADED)
              .setFileSize(size)
              .setLocalUri(uri);
      packet.files.add(file);
    }
    return packet;
  }
}
