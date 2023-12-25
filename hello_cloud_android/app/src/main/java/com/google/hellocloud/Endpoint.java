package com.google.hellocloud;

import static com.google.hellocloud.Util.logErrorAndToast;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.MediaStore;
import androidx.annotation.NonNull;
import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;
import com.google.android.gms.nearby.Nearby;
import com.google.android.gms.nearby.connection.Payload;
import com.google.android.gms.nearby.connection.PayloadTransferUpdate;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
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

  //  void uploadFiles() {
  //    for (OutgoingFile file : outgoingFiles) {
  //      if (file.getState() == OutgoingFile.State.PICKED) {
  //        Instant beginTime = Instant.now();
  //        file.upload()
  //            .addOnSuccessListener(
  //                result -> {
  //                  Log.v(TAG, String.format("Upload succeeded. Remote path: %s",
  // file.remotePath));
  //
  //                  Instant endTime = Instant.now();
  //                  Duration duration = Duration.between(beginTime, endTime);
  //                  TransferViewModel transfer =
  //                      TransferViewModel.upload(
  //                          file.remotePath,
  //                          TransferViewModel.Result.SUCCESS,
  //                          file.fileSize,
  //                          duration);
  //                  addTransfer(transfer);
  //                })
  //            .addOnFailureListener(
  //                error -> {
  //                  logErrorAndToast(
  //                      Main.shared.context,
  //                      R.string.error_toast_cannot_upload,
  //                      error.getMessage());
  //
  //                  TransferViewModel transfer =
  //                      TransferViewModel.upload(
  //                          file.remotePath, TransferViewModel.Result.FAILURE, file.fileSize,
  // null);
  //                  addTransfer(transfer);
  //                })
  //            .continueWith(
  //                result -> {
  //                  notifyPropertyChanged(BR.canPick);
  //                  notifyPropertyChanged(BR.canUpload);
  //                  notifyPropertyChanged(BR.canSend);
  //                  return null;
  //                });
  //      }
  //    }
  //  }

  void sendPacket(Context context, List<Uri> uris) {
    if (getState() != Endpoint.State.CONNECTED) {
      return;
    }
    setState(Endpoint.State.SENDING);

    Packet<OutgoingFile> packet = new Packet<>();
    // TODO: get notification token
    packet.notificationToken = "";
    packet.state = Packet.State.LOADED;
    packet.receiver = name;
    packet.sender = Main.shared.getLocalEndpointName();

    ContentResolver resolver = context.getContentResolver();

    for (Uri uri : uris) {
      String mimeType = resolver.getType(uri);

      // Get the file size.
      Cursor cursor =
          resolver.query(uri, new String[] {MediaStore.MediaColumns.SIZE}, null, null, null);

      assert cursor != null;
      int sizeIndex = cursor.getColumnIndex(MediaStore.MediaColumns.SIZE);
      cursor.moveToFirst();

      int size = cursor.getInt(sizeIndex);
      cursor.close();

      // Construct a file to be added to the packet
      OutgoingFile file =
          new OutgoingFile(mimeType)
              .setState(OutgoingFile.State.LOADED)
              .setFileSize(size)
              .setLocalUri(uri);
      packet.files.add(file);
    }

    // Serialize the packet. Note that we want the files to be serialized as a dictionary, with the
    // id being the key, for easy indexing in Firebase database
    GsonBuilder gson = new GsonBuilder();
    JsonObject result = new JsonObject();
    gson.registerTypeAdapter(packet.files.getClass(), new Packet.OutgoingFilesSerializer());
    result.add("type", new JsonPrimitive("packet"));
    result.add("data", gson.create().toJsonTree(packet));
    String json = result.toString();

    System.out.println(json);

    Payload payload = Payload.fromBytes(json.getBytes(StandardCharsets.UTF_8));
    Nearby.getConnectionsClient(Main.shared.context)
        .sendPayload(id, payload)
        .addOnFailureListener(
            e -> {
              logErrorAndToast(Main.shared.context, R.string.error_toast_cannot_send_payload, e);
              setState(Endpoint.State.CONNECTED);
            });

    setState(State.CONNECTED);
  }

  public void onPacketTransferUpdate(int status) {
    if (status == PayloadTransferUpdate.Status.IN_PROGRESS) {
      return;
    }
    if (state == State.SENDING) {
      setState(State.CONNECTED);
    }
  }

  public void onFilesReceived(String json) {
    //    setState(State.RECEIVING);
    //    IncomingFile[] files = IncomingFile.decodeIncomingFiles(json);
    //    for (IncomingFile file : files) {
    //      TransferViewModel transfer =
    //          TransferViewModel.receive(
    //              file.remotePath, TransferViewModel.Result.SUCCESS, this.toString());
    //      addTransfer(transfer);
    //      file.setState(IncomingFile.State.RECEIVED);
    //    }
    //    // We intentionally do not clean incoming files, since some may not have been downloaded
    // yet
    //    incomingFiles.addAll(Arrays.asList(files));
    //    notifyPropertyChanged(BR.incomingFiles);
    //    notifyPropertyChanged(BR.canDownload);
    //  }
    //
    //  public void downloadFiles() {
    //    for (IncomingFile file : incomingFiles) {
    //      if (file.getState() == IncomingFile.State.RECEIVED) {
    //        Instant beginTime = Instant.now();
    //        file.download()
    //            .addOnSuccessListener(
    //                result -> {
    //                  Instant endTime = Instant.now();
    //                  Duration duration = Duration.between(beginTime, endTime);
    //                  TransferViewModel transfer =
    //                      TransferViewModel.download(
    //                          file.remotePath,
    //                          TransferViewModel.Result.SUCCESS,
    //                          file.fileSize,
    //                          duration);
    //                  addTransfer(transfer);
    //                })
    //            .addOnFailureListener(
    //                error -> {
    //                  logErrorAndToast(
    //                      Main.shared.context,
    //                      R.string.error_toast_cannot_download,
    //                      error.getMessage());
    //
    //                  TransferViewModel transfer =
    //                      TransferViewModel.download(
    //                          file.remotePath, TransferViewModel.Result.FAILURE, file.fileSize,
    // null);
    //                  addTransfer(transfer);
    //                });
    //      }
    //    }
  }

  @NonNull
  @Override
  public String toString() {
    return id + " (" + name + ")";
  }
}
