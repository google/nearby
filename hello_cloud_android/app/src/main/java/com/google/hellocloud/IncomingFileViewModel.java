package com.google.hellocloud;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.MediaStore;

import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import com.google.firebase.storage.FileDownloadTask;
import com.google.gson.Gson;

import java.io.File;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

public final class IncomingFileViewModel extends BaseObservable {
  enum State {
    RECEIVED,
    DOWNLOADING,
    DOWNLOADED
  }

  public String mimeType;
  public String fileName;
  public String remotePath;
  public int fileSize;

  // Do not serialize state
  private transient State state;

  public State getState() {
    return state;
  }

  public IncomingFileViewModel setState(State value) {
    state = value;
    notifyPropertyChanged(BR.isBusy);
    return this;
  }

  @Bindable
  public boolean getIsBusy() {
    return state == State.DOWNLOADING;
  }

  @Bindable
  public Drawable getStateIcon() {
    if (state == null) {
      return null;
    }

    int resource;
    switch (state) {
      case RECEIVED:
        resource = R.drawable.received;
        break;
      case DOWNLOADED:
        resource = R.drawable.downloaded;
        break;
      default:
        return null;
    }
    return MainViewModel.shared.context.getResources().getDrawable(resource, null);
  }

  public IncomingFileViewModel(
      String mimeType, String fileName, String remotePath, int fileSize) {
    this.mimeType = mimeType;
    this.fileName = fileName;
    this.fileSize = fileSize;
    this.remotePath = remotePath;
  }

  public void download(
      OnSuccessListener<FileDownloadTask.TaskSnapshot> successListener,
      OnFailureListener failureListener) {
    if (state != State.RECEIVED) {
      failureListener.onFailure(new Exception("Already downloaded or being downloaded."));
    }

    setState(State.DOWNLOADING);

    // Create a local uri
    ContentResolver resolver = MainViewModel.shared.context.getContentResolver();
    Uri uriFolder = MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);

    File file = new File(uriFolder.getPath(), fileName);

//    ContentValues content = new ContentValues();
//    content.put(MediaStore.Downloads.DISPLAY_NAME, fileName);
//    Uri uri = resolver.insert(uriFolder, content);

    CloudStorage.shared
        .download(remotePath, file)
        .addOnSuccessListener(
            result -> {
              setState(State.DOWNLOADED);
            })
        .addOnFailureListener(
            exception -> {
              setState(State.RECEIVED);
            })
        .addOnSuccessListener(successListener)
        .addOnFailureListener(failureListener);
  }

  public static IncomingFileViewModel[] decodeIncomingFiles(String json) {
    Gson gson = new Gson();
    return gson.fromJson(json, IncomingFileViewModel[].class);
  }
}
