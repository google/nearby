package com.google.hellocloud;

import android.graphics.drawable.Drawable;

import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;
import java.util.ArrayList;
import java.util.UUID;

public class Packet<T extends File> extends BaseObservable {
  public enum State {
    UNKNOWN,
    LOADED,
    UPLOADING,
    UPLOADED,
    RECEIVED,
    DOWNLOADING,
    DOWNLOADED
  }

  public UUID id;
  public String notificationToken;
  public ArrayList<T> files = new ArrayList<>();
  public String receiver;
  public String sender;

  public transient State state;

  public Packet() {
    id = UUID.randomUUID();
  }

  public Packet(UUID uuid) {
    this.id = uuid;
  }

  public Packet<T> setState(State state) {
    this.state = state;
    notifyPropertyChanged(BR.isBusy);
    notifyPropertyChanged(BR.outgoingStateIcon);
    notifyPropertyChanged(BR.incomingStateIcon);
    notifyPropertyChanged(BR.canUpload);
    notifyPropertyChanged(BR.canDownload);
    return this;
  }

  public String getOutgoingDescription() {
    return "Packet" + (receiver == null ? "" : (" for " + receiver));
  }

  public String getIncomingDescription() {
    return "Packet" + (sender == null ? "" : (" from " + sender));
  }

  @Bindable
  public boolean getIsBusy() {
    return state == State.DOWNLOADING || state == State.UPLOADING;
  }

  @Bindable
  public Drawable getOutgoingStateIcon() {
    if (state == null) {
      return null;
    }
    // LOADED: upload button
    // UPLOADED: green filled circle
    // UPLOADING: spinner
    int resource;
    switch (state) {
      case UPLOADED -> resource = R.drawable.uploaded_outgoing;
      default -> {
        return null;
      }
    }
    return Main.shared.context.getResources().getDrawable(resource, null);
  }

  @Bindable
  public Drawable getIncomingStateIcon() {
    if (state == null) {
      return null;
    }
    // RECEIVED: grey dotted circle
    // UPLOADED: download button
    // DOWNLOADING: spinner
    // DOWNLOADED: green filled circle
    int resource;
    switch (state) {
      case RECEIVED -> resource = R.drawable.received;
      case DOWNLOADED -> resource = R.drawable.downloaded;
      default -> {
        return null;
      }
    }
    return Main.shared.context.getResources().getDrawable(resource, null);
  }

  @Bindable
  public boolean getCanUpload() {
    return state == State.LOADED;
  }

  @Bindable
  public boolean getCanDownload() {
    return state == State.UPLOADED;
  }

  public void upload() {
    System.out.println("Uploading...");
  }

  public void download() {
    System.out.print("Downloading...");
  }
}
