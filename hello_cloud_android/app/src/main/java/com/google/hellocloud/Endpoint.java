package com.google.hellocloud;

import static com.google.hellocloud.Utils.TAG;
import static com.google.hellocloud.Utils.logErrorAndToast;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;
import com.google.android.gms.nearby.Nearby;
import com.google.android.gms.nearby.connection.Payload;
import com.google.android.gms.nearby.connection.PayloadTransferUpdate;
import java.nio.charset.StandardCharsets;
import java.util.List;

public final class Endpoint extends BaseObservable {
  public enum State {
    DISCOVERED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    SENDING,
    RECEIVING;

    @NonNull
    @Override
    public String toString() {
      switch (this) {
        case DISCOVERED:
          return "Discovered";
        case DISCONNECTING:
          return "Disconnecting";
        case CONNECTING:
          return "Connecting";
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
  public boolean isIncoming;
  private State state = State.DISCOVERED;
  private String notificationToken;

  public Endpoint(String id, String name) {
    this.id = id;
    this.name = name;
  }

  public Endpoint(String id, String name, boolean isIncoming) {
    this.id = id;
    this.name = name;
    this.isIncoming = isIncoming;
  }

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

  public Endpoint setState(State value) {
    state = value;
    notifyPropertyChanged(BR.state);
    notifyPropertyChanged(BR.stateIcon);
    notifyPropertyChanged(BR.isBusy);
    return this;
  }

  @Bindable
  public Drawable getStateIcon() {
    int resource;
    switch (state) {
      case CONNECTED -> resource = R.drawable.connected;
      case DISCOVERED -> resource = R.drawable.discovered;
      default -> {
        return null;
      }
    }
    return Main.shared.context.getResources().getDrawable(resource, null);
  }

  void loadSendAndUpload(Context context, List<Uri> uris) {
    if (getState() != Endpoint.State.CONNECTED) {
      return;
    }
    setState(Endpoint.State.SENDING);

    Packet<OutgoingFile> packet = Utils.loadPhotos(context, uris, name, notificationToken);
    // Serialize the packet. Note that we want the files to be serialized as a dictionary, with the
    // id being the key, for easy indexing in Firebase database
    DataWrapper<OutgoingFile> wrapper = new DataWrapper<>(packet);
    String json = DataWrapper.getGson().toJson(wrapper);

    Payload payload = Payload.fromBytes(json.getBytes(StandardCharsets.UTF_8));
    Nearby.getConnectionsClient(Main.shared.context)
        .sendPayload(id, payload)
        .addOnCompleteListener(
            task -> {
              if (!task.isSuccessful()) {
                logErrorAndToast(
                    Main.shared.context,
                    R.string.error_toast_cannot_send_payload,
                    task.getException());
              }
              setState(State.CONNECTED);
            });
    Main.shared.addOutgoingPacket(packet);
    packet.upload();
  }

  public void onPacketTransferUpdate(int status) {
    if (status == PayloadTransferUpdate.Status.IN_PROGRESS) {
      return;
    }
    if (state == State.SENDING) {
      setState(State.CONNECTED);
    }
  }

  public void onNotificationTokenReceived(String token) {
    Log.i(TAG, "Notification token received: " + token);
    this.notificationToken = token;
  }

  public void onPacketReceived(Packet<IncomingFile> packet) {
    Log.i(TAG, "Packet received: " + packet.id);
    packet.sender = name;
    packet.setState(Packet.State.RECEIVED);
    Main.shared.addIncomingPacket(packet);
    Main.shared.flashPacket(packet);

    CloudDatabase.shared.observePacket(
        packet.id,
        newPacket -> {
          packet.update(newPacket);
          Main.shared.flashPacket(packet);
          packet.download(Main.shared.context);
          return null;
        });
  }

  @NonNull
  @Override
  public String toString() {
    return id + " (" + name + ")";
  }
}
