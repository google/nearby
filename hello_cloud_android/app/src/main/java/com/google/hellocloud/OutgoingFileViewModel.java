package com.google.hellocloud;

import android.graphics.drawable.Drawable;
import android.net.Uri;
import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;
import com.google.gson.Gson;
import java.util.List;

public final class OutgoingFileViewModel extends BaseObservable {
  enum State {
    PICKED,
    LOADING,
    LOADED,
    UPLOADING,
    UPLOADED
  }

  public String mimeType;
  public String fileName;
  public String remotePath;
  public int fileSize;

  // Do not serialize state
  private transient State state;
  private transient Uri localUri;

  public State getState() {
    return state;
  }

  public OutgoingFileViewModel setState(State value) {
    state = value;
    notifyPropertyChanged(BR.stateIcon);
    notifyPropertyChanged(BR.isBusy);
    return this;
  }

  public OutgoingFileViewModel setLocalUri(Uri uri) {
    localUri = uri;
    return this;
  }

  @Bindable
  public boolean getIsBusy() {
    return state == State.UPLOADING || state == State.LOADING;
  }

  @Bindable
  public Drawable getStateIcon() {
    if (state == null) {
      return null;
    }

    int resource;
    switch (state) {
      case LOADED:
        resource = R.drawable.loaded;
        break;
      case PICKED:
        resource = R.drawable.picked;
        break;
      case UPLOADED:
        resource = R.drawable.uploaded;
        break;
      default:
        return null;
    }
    return MainViewModel.shared.context.getResources().getDrawable(resource, null);
  }

  public OutgoingFileViewModel(String mimeType, String fileName, String remotePath, int fileSize) {
    this.mimeType = mimeType;
    this.fileName = fileName;
    this.fileSize = fileSize;
    this.remotePath = remotePath;
  }

  public static String encodeOutgoingFiles(List<OutgoingFileViewModel> files) {
    return (new Gson()).toJson(files);
  }

  public void upload() {
    CloudStorage.shared
        .upload(this.remotePath, this.localUri)
        .addOnSuccessListener(
            result -> {
              System.out.println("Upload completed!" + result);
            })
        .addOnFailureListener(
            error -> {
              System.out.println("Upload failed!" + error);
            });
  }
}
