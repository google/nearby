package com.google.hellocloud;

import android.graphics.drawable.Drawable;

import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;

public final class OutgoingFileViewModel extends BaseObservable {
  enum State {
    PICKED, LOADING, LOADED, UPLOADING, UPLOADED
  }

  public String mimeType;
  public String fileName;
  public String remotePath;
  public int fileSize;

  public State state;

  public void setState(State value) {
    state = value;
    notifyPropertyChanged(BR.stateIcon);
    notifyPropertyChanged(BR.isBusy);
  }

  @Bindable
  public boolean getIsBusy() {
    return state == State.UPLOADING || state == State.LOADING;
  }

  @Bindable
  public Drawable getStateIcon() {
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

  public OutgoingFileViewModel(String mimeType, String fileName, String remotePath, int fileSize, State state) {
    this.mimeType = mimeType;
    this.fileName = fileName;
    this.fileSize = fileSize;
    this.state = state;
    this.remotePath = remotePath;
  }

  public void upload() {

  }
}
