package com.google.hellocloud;

import androidx.annotation.NonNull;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.firebase.storage.FileDownloadTask;
import com.google.firebase.storage.FirebaseStorage;
import com.google.firebase.storage.StorageReference;

import java.io.File;

public class CloudStorage {
  static public CloudStorage shared = new CloudStorage();

  FirebaseStorage storage;
  StorageReference storageRef;

  private CloudStorage() {
    storage = FirebaseStorage.getInstance();
    storageRef = storage.getReference();
  }

  public void download(String remotePath, String fileName) {
    StorageReference fileRef = storageRef.child(remotePath);
    File localFile = File.createTempFile("images", "jpg");

    fileRef.getFile(localFile).addOnSuccessListener(new OnSuccessListener<FileDownloadTask.TaskSnapshot>() {
      @Override
      public void onSuccess(FileDownloadTask.TaskSnapshot taskSnapshot) {
        // Local temp file has been created
      }
    }).addOnFailureListener(new OnFailureListener() {
      @Override
      public void onFailure(@NonNull Exception exception) {
        // Handle any errors
      }
    });
  }
}
