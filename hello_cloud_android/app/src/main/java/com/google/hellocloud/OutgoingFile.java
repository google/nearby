package com.google.hellocloud;

import android.graphics.drawable.Drawable;
import android.net.Uri;
import androidx.annotation.NonNull;
import androidx.databinding.Bindable;
import com.google.android.gms.tasks.Task;
import java.util.UUID;

public final class OutgoingFile extends File {
  enum State {
    // On Android, we don't have PICKED and LOADING. I need to research on how to get rid of them
    // on iOS.
    LOADED,
    UPLOADING,
    UPLOADED
  }

  public String remotePath;

  // Do not serialize
  private transient State state;
  private transient Uri localUri;

  public State getState() {
    return state;
  }

  public OutgoingFile setState(State value) {
    state = value;
    notifyPropertyChanged(BR.stateIcon);
    notifyPropertyChanged(BR.isBusy);
    return this;
  }

  public OutgoingFile setFileSize(long fileSize) {
    this.fileSize = fileSize;
    return this;
  }

  public OutgoingFile setLocalUri(Uri uri) {
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
      // LOADED: grey filled circle
      // UPLOADED: green filled circle
      // UPLOADING: spinner
      case LOADED -> resource = R.drawable.loaded;
      case UPLOADED -> resource = R.drawable.uploaded_outgoing;
      default -> {
        return null;
      }
    }
    return Main.shared.context.getResources().getDrawable(resource, null);
  }

  public OutgoingFile(String mimeType) {
    this.mimeType = mimeType;
    this.id = UUID.randomUUID();
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
        .addOnSuccessListener(result -> setState(State.UPLOADED), error -> setState(State.LOADED));
  }
}
