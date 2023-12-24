package com.google.hellocloud;

import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;

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
//  private final List<IncomingFile> incomingFiles = new ArrayList<>();
//  private final List<OutgoingFile> outgoingFiles = new ArrayList<>();
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

  public void setState(State value) {
    state = value;
    notifyPropertyChanged(BR.state);
    notifyPropertyChanged(BR.stateIcon);
    notifyPropertyChanged(BR.isBusy);
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

//  @Bindable
//  public List<OutgoingFile> getOutgoingFiles() {
//    return outgoingFiles;
//  }
//
//  @Bindable
//  public List<IncomingFile> getIncomingFiles() {
//    return incomingFiles;
//  }
//



//  public void onMediaPicked(List<OutgoingFile> files) {
//    // The UI triggers a media picker and up completion, calls us.
//    outgoingFiles.clear();
//    outgoingFiles.addAll(files);
//    notifyPropertyChanged(BR.outgoingFiles);
//    notifyPropertyChanged(BR.canPick);
//    notifyPropertyChanged(BR.canUpload);
//    notifyPropertyChanged(BR.canSend);
//  }

//  void uploadFiles() {
//    for (OutgoingFile file : outgoingFiles) {
//      if (file.getState() == OutgoingFile.State.PICKED) {
//        Instant beginTime = Instant.now();
//        file.upload()
//            .addOnSuccessListener(
//                result -> {
//                  Log.v(TAG, String.format("Upload succeeded. Remote path: %s", file.remotePath));
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
//                          file.remotePath, TransferViewModel.Result.FAILURE, file.fileSize, null);
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

//  void sendFiles() {
//    if (getState() != Endpoint.State.CONNECTED) {
//      return;
//    }
//    String json = OutgoingFile.encodeOutgoingFiles(outgoingFiles);
//    setState(Endpoint.State.SENDING);
//
//    Payload payload = Payload.fromBytes(json.getBytes(StandardCharsets.UTF_8));
//    Nearby.getConnectionsClient(Main.shared.context)
//        .sendPayload(id, payload)
//        .addOnFailureListener(
//            e -> {
//              logErrorAndToast(
//                  Main.shared.context, R.string.error_toast_cannot_send_payload, e);
//              setState(Endpoint.State.CONNECTED);
//              for (OutgoingFile file : outgoingFiles) {
//                TransferViewModel transfer =
//                    TransferViewModel.send(
//                        file.remotePath, TransferViewModel.Result.FAILURE, this.toString());
//                addTransfer(transfer);
//              }
//            });
//  }

  public void onFilesSendUpdate(int status) {
//    if (status == PayloadTransferUpdate.Status.IN_PROGRESS) {
//      return;
//    }
//
//    if (status == PayloadTransferUpdate.Status.SUCCESS) {
//      for (OutgoingFile file : outgoingFiles) {
//        TransferViewModel transfer =
//            TransferViewModel.send(
//                file.remotePath, TransferViewModel.Result.SUCCESS, this.toString());
//        addTransfer(transfer);
//      }
//      boolean isSending = getState() == Endpoint.State.SENDING;
//      setState(Endpoint.State.CONNECTED);
//      if (isSending) {
//        outgoingFiles.clear();
//        notifyPropertyChanged(BR.outgoingFiles);
//        notifyPropertyChanged(BR.canPick);
//        notifyPropertyChanged(BR.canUpload);
//        notifyPropertyChanged(BR.canSend);
//      }
//    } else {
//      for (OutgoingFile file : outgoingFiles) {
//        TransferViewModel transfer =
//            TransferViewModel.send(
//                file.remotePath, TransferViewModel.Result.FAILURE, this.toString());
//        addTransfer(transfer);
//      }
//      setState(Endpoint.State.CONNECTED);
//    }
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
//    // We intentionally do not clean incoming files, since some may not have been downloaded yet
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
//                          file.remotePath, TransferViewModel.Result.FAILURE, file.fileSize, null);
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
