package com.google.hellocloud;

import android.content.ContentResolver;
import android.net.Uri;
import com.google.android.gms.tasks.Task;
import com.google.firebase.storage.FirebaseStorage;
import com.google.firebase.storage.StorageReference;
import java.io.OutputStream;

public class CloudStorage {
  public static CloudStorage shared = new CloudStorage();
  ;

  FirebaseStorage storage;
  StorageReference storageRef;

  private CloudStorage() {
    storage = FirebaseStorage.getInstance();
    storageRef = storage.getReference();
  }

  public Task<Void> download(String remotePath, Uri localUri) {
    StorageReference fileRef = storageRef.child(remotePath);

    return fileRef
        .getBytes(1024 * 1024 * 16)
        .continueWith(
            result -> {
              if (result.isSuccessful()) {
                ContentResolver resolver = Main.shared.context.getContentResolver();
                OutputStream stream = resolver.openOutputStream(localUri);
                assert stream != null;
                stream.write(result.getResult());
                stream.close();
                return null;
              } else {
                throw result == null ? new Exception("Unknown error") : result.getException();
              }
            });
  }

  public Task<Void> upload(String remotePath, Uri localUri) {
    StorageReference fileRef = storageRef.child(remotePath);
    return fileRef
        .putFile(localUri)
        .continueWith(
            result -> {
              if (result.isSuccessful()) {
                return null;
              } else {
                throw result == null ? new Exception("Unknown error") : result.getException();
              }
            });
  }
}
