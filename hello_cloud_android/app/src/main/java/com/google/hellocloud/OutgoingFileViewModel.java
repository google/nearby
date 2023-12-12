package com.google.hellocloud;

import android.graphics.drawable.Drawable;

import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;

import com.google.gson.Gson;

import java.util.List;

public final class OutgoingFileViewModel extends BaseObservable {
  enum State {
    PICKED, LOADING, LOADED, UPLOADING, UPLOADED
  }

  public String mimeType;
  public String fileName;
  public String remotePath;
  public int fileSize;

  // Do not serialize state
  public transient State state;

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

  public OutgoingFileViewModel(String mimeType, String fileName, String remotePath, int fileSize, State state) {
    this.mimeType = mimeType;
    this.fileName = fileName;
    this.fileSize = fileSize;
    this.state = state;
    this.remotePath = remotePath;
  }

  static public String encodeOutgoingFiles(List<OutgoingFileViewModel> files) {
    return (new Gson()).toJson(files);
  }

  public void upload() {

  }
}
