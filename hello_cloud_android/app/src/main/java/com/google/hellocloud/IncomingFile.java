package com.google.hellocloud;

import android.graphics.drawable.Drawable;
import android.net.Uri;
import androidx.databinding.Bindable;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import com.google.gson.Gson;

public final class IncomingFile extends File {
  enum State {
    RECEIVED,
    UPLOADED,
    DOWNLOADING,
    DOWNLOADED
  }

  public String remotePath;

  // Do not serialize
  private transient State state;
  private transient Uri localUri;

  public State getState() {
    return state;
  }

  public IncomingFile setState(State value) {
    state = value;
    notifyPropertyChanged(BR.stateIcon);
    notifyPropertyChanged(BR.isBusy);
    return this;
  }

  public IncomingFile setFileSize(long fileSize) {
    this.fileSize = fileSize;
    return this;
  }

  public IncomingFile setLocalUri(Uri uri) {
    this.localUri = uri;
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

    // RECEIVED: grey dotted circle
    // UPLOADED: grey filled circle
    // DOWNLOADED: green filled circle
    // DOWNLOADING: spinner

    int resource;
    switch (state) {
      case RECEIVED -> resource = R.drawable.received;
      case UPLOADED -> resource = R.drawable.uploaded_incoming;
      case DOWNLOADED -> resource = R.drawable.downloaded;
      default -> {
        return null;
      }
    }
    return Main.shared.context.getResources().getDrawable(resource, null);
  }

  public IncomingFile(String mimeType) {
    this.mimeType = mimeType;
  }

  public Task<Long> download() {
    if (state != State.UPLOADED) {
      return Tasks.forResult(null);
    }

    setState(State.DOWNLOADING);
    assert (localUri != null);
    return CloudStorage.shared
        .download(remotePath, localUri)
        .addOnSuccessListener(result -> setState(State.DOWNLOADED))
        .addOnFailureListener(error -> setState(State.RECEIVED));
  }
}
