package com.google.hellocloud;

import android.graphics.drawable.Drawable;

import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;

import com.google.gson.Gson;

public final class IncomingFileViewModel extends BaseObservable {
  enum State {
    RECEIVED, DOWNLOADING, DOWNLOADED
  }

  public String mimeType;
  public String fileName;
  public String remotePath;
  public int fileSize;

  // Do not serialize state
  private transient State state;

  public State getState() { return state; }
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

  public IncomingFileViewModel(String mimeType, String fileName, String remotePath, int fileSize, State state) {
    this.mimeType = mimeType;
    this.fileName = fileName;
    this.fileSize = fileSize;
    this.state = state;
    this.remotePath = remotePath;
  }

  static public IncomingFileViewModel[] decodeIncomingFiles(String json) {
    Gson gson = new Gson();
    return gson.fromJson(json, IncomingFileViewModel[].class);
  }
}
