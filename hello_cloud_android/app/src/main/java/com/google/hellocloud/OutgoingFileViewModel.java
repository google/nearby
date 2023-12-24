package com.google.hellocloud;

import android.graphics.drawable.Drawable;
import android.net.Uri;
import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;
import com.google.android.gms.tasks.Task;
import com.google.gson.Gson;
import java.util.List;
import java.util.UUID;

public final class OutgoingFileViewModel extends BaseObservable {
  enum State {
    PICKED,
    UPLOADING,
    UPLOADED
  }

  public String mimeType;
  public String fileName;
  public String remotePath;
  public int fileSize;

  // Do not serialize
  private transient State state;
  private transient Uri localUri;

  public State getState() {
    return state;
  }

  public OutgoingFileViewModel setState(State value) {
    state = value;
    notifyPropertyChanged(BR.stateIcon);
    notifyPropertyChanged(BR.isBusy);
    notifyPropertyChanged(BR.canPick);
//    notifyPropertyChanged(BR.canUpload);
//    notifyPropertyChanged(BR.canSend);
    return this;
  }

  public OutgoingFileViewModel setLocalUri(Uri uri) {
    localUri = uri;
    return this;
  }

  @Bindable
  public boolean getIsBusy() {
    return state == State.UPLOADING;
  }

  @Bindable
  public Drawable getStateIcon() {
    if (state == null) {
      return null;
    }

    int resource;
    switch (state) {
      case PICKED -> resource = R.drawable.picked;
      case UPLOADED -> resource = R.drawable.uploaded;
      default -> {
        return null;
      }
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

  public Task<Void> upload() {
    setState(State.UPLOADING);
    remotePath = UUID.randomUUID().toString().toUpperCase();
    String ext = null;
    if ("image/jpeg".equals(mimeType)) {
      ext = "jpeg";
    } else if ("image/png".equals(mimeType)) {
      ext = "png";
    }

    if (ext != null) {
      remotePath += "." + ext;
    }

    return CloudStorage.shared
        .upload(remotePath, localUri)
        .addOnSuccessListener(result -> setState(State.UPLOADED), error -> setState(State.PICKED));
  }
}
