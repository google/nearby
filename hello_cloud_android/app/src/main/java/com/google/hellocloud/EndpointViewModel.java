package com.google.hellocloud;

import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;

import java.util.ArrayList;
import java.util.List;

public final class EndpointViewModel extends BaseObservable {
  public enum State {
    DISCOVERED, PENDING, CONNECTED, SENDING, RECEIVING;

    @NonNull
    @Override
    public String toString() {
      switch (this) {
        case DISCOVERED:
          return "Discovered";
        case PENDING:
          return "Pending";
        case CONNECTED:
          return "Connected";
        case SENDING:
          return "Sending";
        case RECEIVING:
          return "Receiving";
      }
      return "UNKNOWN";
    }
  }

  public final String name;
  public final String id;
  private final List<IncomingFileViewModel> incomingFiles = new ArrayList<>();
  private final List<OutgoingFileViewModel> outgoingFiles = new ArrayList<>();
  public final List<TransferViewModel> transfers = new ArrayList<>();
  public boolean isIncoming;

  private State state = State.DISCOVERED;


  @Bindable
  public String getId() {
    return id;
  }

  @Bindable
  public String getName() {
    return name;
  }

  @Bindable
  public State getState() {
    return state;
  }

  public void setState(State value) {
    state = value;
    notifyPropertyChanged(BR.state);
    notifyPropertyChanged(BR.stateIcon);
    notifyPropertyChanged(BR.isBusy);
  }

  @Bindable
  public boolean getIsBusy() {
    return state == State.RECEIVING || state == State.SENDING || state == State.PENDING;
  }

  @Bindable
  public Drawable getStateIcon() {
    int resource;
    switch (state) {
      case CONNECTED:
        resource = R.drawable.connected;
        break;
      case DISCOVERED:
        resource = R.drawable.discovered;
        break;
      default:
        return null;
    }
    return MainViewModel.shared.context.getResources().getDrawable(resource, null);
  }

  @Bindable
  public List<OutgoingFileViewModel> getOutgoingFiles() {
    return outgoingFiles;
  }

  @Bindable
  public List<IncomingFileViewModel> getIncomingFiles() {
    return incomingFiles;
  }

  @Bindable
  public List<TransferViewModel> getTransfers() {
    return transfers;
  }

  public EndpointViewModel(String id, String name) {
    this.id = id;
    this.name = name;
  }

  public EndpointViewModel(String id, String name, boolean isIncoming) {
    this.id = id;
    this.name = name;
    this.isIncoming = isIncoming;
  }

  public void onMediaPicked(List<OutgoingFileViewModel> files) {
    outgoingFiles.clear();
    outgoingFiles.addAll(files);
    notifyPropertyChanged(BR.outgoingFiles);
  }

  public void onFilesReceived(List<IncomingFileViewModel> files) {
    // We intentionally do not clean incoming files, since some may not have been downloaded yet
    incomingFiles.addAll(files);
    notifyPropertyChanged(BR.incomingFiles);
  }

  @NonNull
  @Override
  public String toString() {
    return id + " (" + name + ")";
  }
}
