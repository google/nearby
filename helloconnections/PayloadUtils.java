package com.google.location.nearby.apps.helloconnections;

import static com.google.location.nearby.apps.helloconnections.Util.TAG;
import static com.google.location.nearby.apps.helloconnections.Util.generateRandomBytes;

import android.content.Context;
import android.os.SystemClock;
import android.util.Log;
import androidx.annotation.Nullable;
import com.google.android.gms.nearby.connection.Payload;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.MalformedURLException;

/** A util class to provide methods to create and send file payloads. */
class PayloadUtils {

  static Payload createBytesPayload(int size) {
    return Payload.fromBytes(generateRandomBytes(size));
  }

  @Nullable
  static Payload createFilePayload(Context context, int size) {
    File file = saveDataAsFile(context, size);
    if (file == null) {
      return null;
    }

    // Next, write a specified amount of bytes to the file and generate a payload from that.
    try {
      return Payload.fromFile(file);
    } catch (IOException e) {
      Log.e(
          TAG,
          String.format("Encountered an error while creating a file payload of size %d.", size),
          e);
    }

    return null;
  }

  @Nullable
  static Payload createStreamPayload(Context context, int size) {
    File file = saveDataAsFile(context, size);
    if (file == null) {
      return null;
    }

    try {
      return Payload.fromStream(file.toURI().toURL().openStream());
    } catch (SecurityException se) {
      Log.e(TAG, "Encountered an error while calling File#toURI().", se);
    } catch (MalformedURLException | IllegalArgumentException e) {
      Log.e(TAG, "Encountered an error while calling URI#toURL().", e);
    } catch (IOException ioe) {
      Log.e(TAG, "Encountered an error while calling URL#openStream().", ioe);
    }
    return null;
  }

  @Nullable
  private static File saveDataAsFile(Context context, int size) {
    String filename = Long.toString(SystemClock.elapsedRealtime());

    // First, create the file.
    File file;
    try {
      file = File.createTempFile(filename, /* suffix= */ null, context.getCacheDir());
    } catch (IOException e) {
      Log.e(TAG, "Encountered an error while creating a file.", e);
      return null;
    }

    // Next, write a specified amount of bytes to the file and generate a payload from that.
    try (FileOutputStream fileOutputStream = new FileOutputStream(file)) {
      byte[] randomBytes = null;
      int count = 0;
      while (count < size) {
        if ((size - count) < 1024) {
          randomBytes = generateRandomBytes(size - count);
        } else {
          randomBytes = generateRandomBytes(1024);
        }

        fileOutputStream.write(randomBytes);
        count += randomBytes.length;
      }

      return file;
    } catch (IOException e) {
      Log.e(TAG, String.format("Encountered an error while creating a file of size %d.", size), e);
    }

    return null;
  }
}
