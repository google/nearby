package com.google.hellocloud;

import static com.google.hellocloud.Util.TAG;
import static com.google.hellocloud.Util.logErrorAndToast;

import android.graphics.drawable.Drawable;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;
import com.google.android.gms.nearby.Nearby;
import com.google.android.gms.nearby.connection.Payload;
import com.google.android.gms.nearby.connection.PayloadTransferUpdate;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class EndpointViewModel extends BaseObservable {
  public enum State {
    DISCOVERED,
    PENDING,
    CONNECTED,
    SENDING,
    RECEIVING;

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

  void uploadFiles() {
    for (OutgoingFileViewModel file : outgoingFiles) {
      if (file.getState() == OutgoingFileViewModel.State.PICKED) {
        Instant beginTime = Instant.now();
        file.upload()
            .addOnSuccessListener(
                result -> {
                  Log.v(TAG, String.format("Upload succeeded. Remote path: %s", file.remotePath));

                  Instant endTime = Instant.now();
                  Duration duration = Duration.between(beginTime, endTime);
                  TransferViewModel transfer =
                      TransferViewModel.upload(
                          file.remotePath,
                          TransferViewModel.Result.SUCCESS,
                          file.fileSize,
                          duration);
                  addTransfer(transfer);
                })
            .addOnFailureListener(
                error -> {
                  logErrorAndToast(
                      MainViewModel.shared.context,
                      R.string.error_toast_cannot_upload,
                      error.getMessage());

                  TransferViewModel transfer =
                      TransferViewModel.upload(
                          file.remotePath, TransferViewModel.Result.FAILURE, file.fileSize, null);
                  addTransfer(transfer);
                });

        //            .addOnFailureListener(
        //                error -> {
        //                  logErrorAndToast(MainViewModel.shared.context,
        // R.string.error_toast_cannot_upload, error.getMessage());
        //                  TransferViewModel transfer =
        //                      TransferViewModel.upload(
        //                          file.remotePath, TransferViewModel.Result.FAILURE,
        // file.fileSize, null);
        //                  addTransfer(transfer);
        //                });
      }
    }
  }

  void sendFiles() {
    if (getState() != EndpointViewModel.State.CONNECTED) {
      return;
    }
    String json = OutgoingFileViewModel.encodeOutgoingFiles(outgoingFiles);
    setState(EndpointViewModel.State.SENDING);

    Payload payload = Payload.fromBytes(json.getBytes(StandardCharsets.UTF_8));
    Nearby.getConnectionsClient(MainViewModel.shared.context)
        .sendPayload(id, payload)
        .addOnFailureListener(
            e -> {
              logErrorAndToast(
                  MainViewModel.shared.context, R.string.error_toast_cannot_send_payload, e);
              setState(EndpointViewModel.State.CONNECTED);
              for (OutgoingFileViewModel file : outgoingFiles) {
                TransferViewModel transfer =
                    TransferViewModel.send(file.remotePath, TransferViewModel.Result.FAILURE, id);
                addTransfer(transfer);
              }
            });
  }

  public void onMediaPicked(List<OutgoingFileViewModel> files) {
    outgoingFiles.clear();
    outgoingFiles.addAll(files);
    notifyPropertyChanged(BR.outgoingFiles);
  }

  public void onFilesReceived(String json) {
    setState(State.RECEIVING);
    IncomingFileViewModel[] files = IncomingFileViewModel.decodeIncomingFiles(json);
    for (IncomingFileViewModel file : files) {
      TransferViewModel transfer =
          TransferViewModel.receive(file.remotePath, TransferViewModel.Result.SUCCESS, id);
      addTransfer(transfer);
      file.setState(IncomingFileViewModel.State.RECEIVED);
    }
    // We intentionally do not clean incoming files, since some may not have been downloaded yet
    incomingFiles.addAll(Arrays.asList(files));
    notifyPropertyChanged(BR.incomingFiles);
  }

  public void onFilesSendUpdate(int status) {
    if (status == PayloadTransferUpdate.Status.IN_PROGRESS) {
      return;
    }

    if (status == PayloadTransferUpdate.Status.SUCCESS) {
      for (OutgoingFileViewModel file : outgoingFiles) {
        TransferViewModel transfer =
            TransferViewModel.send(file.remotePath, TransferViewModel.Result.SUCCESS, id);
        addTransfer(transfer);
      }
      boolean isSending = getState() == EndpointViewModel.State.SENDING;
      setState(EndpointViewModel.State.CONNECTED);
      if (isSending) {
        getOutgoingFiles().clear();
      }
    } else {
      for (OutgoingFileViewModel file : outgoingFiles) {
        TransferViewModel transfer =
            TransferViewModel.send(file.remotePath, TransferViewModel.Result.FAILURE, id);
        addTransfer(transfer);
      }
      setState(EndpointViewModel.State.CONNECTED);
    }
  }

  public void addTransfer(TransferViewModel transfer) {
    transfers.add(transfer);
    notifyPropertyChanged(BR.transfers);
  }

  @NonNull
  @Override
  public String toString() {
    return id + " (" + name + ")";
  }
}
