package com.google.hellocloud;

import android.graphics.drawable.Drawable;
import android.net.Uri;
import androidx.databinding.Bindable;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;

import java.util.UUID;

public final class OutgoingFile extends File {
  enum State {
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

  public Task<Long> upload() {
    if (state != State.LOADED) {
      return Tasks.forResult(null);
    }

    remotePath = UUID.randomUUID().toString().toUpperCase();
    if ("image/jpeg".equals(mimeType)) {
      remotePath += ".jpeg";
    } else if ("image/png".equals(mimeType)) {
      remotePath += ".png";
    }

    setState(State.UPLOADING);
    assert (localUri != null);
    return CloudStorage.shared
        .upload(remotePath, localUri)
        .addOnSuccessListener(result -> setState(State.UPLOADED))
        .addOnFailureListener(error -> setState(State.LOADED));
  }
}
