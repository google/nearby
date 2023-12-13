package com.google.hellocloud;

import android.net.Uri;

import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import com.google.firebase.storage.FileDownloadTask;
import com.google.firebase.storage.FirebaseStorage;
import com.google.firebase.storage.StorageReference;
import java.io.File;

public class CloudStorage {
  public static CloudStorage shared = new CloudStorage();
  ;

  FirebaseStorage storage;
  StorageReference storageRef;

  private CloudStorage() {
    storage = FirebaseStorage.getInstance();
    storageRef = storage.getReference();
  }

  public FileDownloadTask download(String remotePath, File file) {
    StorageReference fileRef = storageRef.child(remotePath);
    return fileRef.getFile(file);
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
                throw new RuntimeException("No!");
              }
            });
  }
}
