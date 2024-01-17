package com.google.hellocloud;

import android.content.ContentResolver;
import android.net.Uri;
import com.google.android.gms.tasks.Task;
import com.google.firebase.storage.FirebaseStorage;
import com.google.firebase.storage.StorageReference;
import java.io.OutputStream;

public class CloudStorage {
  public static CloudStorage shared = new CloudStorage();

  FirebaseStorage storage;
  StorageReference storageRef;

  private CloudStorage() {
    storage = FirebaseStorage.getInstance();
    // TODO: emulator doesn't seem to work for Android although iOS works fine
    if (Config.localCloud) {
      storage.useEmulator(Config.localCloudHost, 9199);
    }
    storageRef = storage.getReference();

    // Set a short timeout for debugging. The default is 600s
    storage.setMaxUploadRetryTimeMillis(3000);
    storage.setMaxDownloadRetryTimeMillis(3000);
  }

  public Task<Long> download(String remotePath, Uri localUri) {
    StorageReference fileRef = storageRef.child(remotePath);

    return fileRef
        .getBytes(1024 * 1024 * 16)
        .continueWith(
            result -> {
              if (result.isSuccessful()) {
                ContentResolver resolver = Main.shared.context.getContentResolver();
                OutputStream stream = resolver.openOutputStream(localUri);
                assert stream != null;
                byte[] bytes = result.getResult();
                stream.write(bytes);
                stream.close();
                return (long) bytes.length;
              } else {
                throw result == null ? new Exception("Unknown error") : result.getException();
              }
            });
  }

  public Task<Long> upload(String remotePath, Uri localUri) {
    StorageReference fileRef = storageRef.child(remotePath);
    return fileRef
        .putFile(localUri)
        .continueWith(
            result -> {
              if (result.isSuccessful()) {
                return (result.getResult().getBytesTransferred());
              } else {
                throw result == null ? new Exception("Unknown error") : result.getException();
              }
            });
  }
}
