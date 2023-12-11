package com.google.hellocloud;

import android.graphics.drawable.Drawable;

import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;

public final class IncomingFileViewModel extends BaseObservable {
  enum State {
    RECEIVED, DOWNLOADING, DOWNLOADED
  }

  public String mimeType;
  public String fileName;
  public String remotePath;
  public int fileSize;

  public State state;

  public void setState(State value) {
    state = value;

    notifyPropertyChanged(BR.isBusy);
  }

  @Bindable
  public boolean getIsBusy() {
    return state == State.DOWNLOADING;
  }

  @Bindable
  public Drawable getStateIcon() {
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

  public IncomingFileViewModel(String mimeType, String fileName, String remotePath, int fileSize, State state) {
    this.mimeType = mimeType;
    this.fileName = fileName;
    this.fileSize = fileSize;
    this.state = state;
    this.remotePath = remotePath;
  }
}
