package com.google.hellocloud;

import android.content.Context;

import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Optional;

public final class MainViewModel extends BaseObservable {
  public static MainViewModel shared = createDebugModel();

  public Context context;

  private String localEndpointId;
  private String localEndpointName;
  private final ArrayList<EndpointViewModel> endpoints = new ArrayList<>();

  private boolean isDiscovering;
  private boolean isAdvertising;

  @Bindable
  public String getLocalEndpointId() {
    return localEndpointId;
  }

  public void setLocalEndpointId(String value) {
    localEndpointId = value;
    notifyPropertyChanged(BR.localEndpointId);
  }

  @Bindable
  public String getLocalEndpointName() {
    return localEndpointName;
  }

  public void setLocalEndpointName(String value) {
    localEndpointName = value;
    notifyPropertyChanged(BR.localEndpointName);
  }

  public boolean getIsAdvertising() {
    return isAdvertising;
  }

  public void setIsAdvertising(boolean value) {
    isAdvertising = value;
    if (value) {
      startAdvertising();
    } else {
      stopAdvertising();
    }
  }

  public boolean getIsDiscovering() {
    return isDiscovering;
  }

  public void setIsDiscovering(boolean value) {
    isDiscovering = value;
    if (value) {
      startDiscovering();
    } else {
      stopDiscovering();
    }
  }

  @Bindable
  public List<EndpointViewModel> getEndpoints() {
    return endpoints;
  }

  void addEndpoint(EndpointViewModel endpoint) {
    endpoints.add(endpoint);
    notifyPropertyChanged(BR.endpoints);
  }

  void startAdvertising() {
  }

  void stopAdvertising() {
  }

  void startDiscovering() {
  }

  void stopDiscovering() {
  }

  void requestConnection(String endpointId) {
    getEndpoint(endpointId);
  }

  void disconnect(String endpointId) {
  }

  public Optional<EndpointViewModel> getEndpoint(String endpointId) {
    assert endpoints != null;
    return endpoints.stream().filter(e -> Objects.equals(e.id, endpointId)).findFirst();
  }

  public static MainViewModel createDebugModel() {
    MainViewModel result = new MainViewModel();
    result.localEndpointId = "KUOW";
    result.localEndpointName = "Pixel 8";

    result.addEndpoint(new EndpointViewModel("R2D2", "Debug droid"));

    EndpointViewModel endpoint2 = new EndpointViewModel("C3PO", "Fuzzy droid");
    endpoint2.getOutgoingFiles().add(new OutgoingFileViewModel(
            "image/jpeg", "IMG_0001.jpeg", "1234", 100000, OutgoingFileViewModel.State.PICKED));
    endpoint2.getOutgoingFiles().add(new OutgoingFileViewModel(
            "image/jpeg", "IMG_0002.jpeg", "5678", 100000, OutgoingFileViewModel.State.LOADING));
    endpoint2.getOutgoingFiles().add(new OutgoingFileViewModel(
            "image/jpeg", "IMG_0003.jpeg", "5678", 100000, OutgoingFileViewModel.State.LOADED));
    endpoint2.getOutgoingFiles().add(new OutgoingFileViewModel(
            "image/jpeg", "IMG_0004.jpeg", "5678", 100000, OutgoingFileViewModel.State.UPLOADED));
    endpoint2.getOutgoingFiles().add(new OutgoingFileViewModel(
            "image/jpeg", "IMG_0005.jpeg", "5678", 100000, OutgoingFileViewModel.State.UPLOADED));

    endpoint2.getIncomingFiles().add(new IncomingFileViewModel(
            "image/jpeg", "IMG_1001.jpeg", "1234", 100000, IncomingFileViewModel.State.RECEIVED));
    endpoint2.getIncomingFiles().add(new IncomingFileViewModel(
            "image/jpeg", "IMG_1002.jpeg", "1234", 100000, IncomingFileViewModel.State.DOWNLOADING));
    endpoint2.getIncomingFiles().add(new IncomingFileViewModel(
            "image/jpeg", "IMG_1003.jpeg", "1234", 100000, IncomingFileViewModel.State.DOWNLOADED));

    endpoint2.getTransfers().add(TransferViewModel.Download(
            "1234", TransferViewModel.Result.SUCCESS, 100000, Duration.ofSeconds(10)));
    endpoint2.getTransfers().add(TransferViewModel.Upload(
            "1234", TransferViewModel.Result.CANCELED, 2000000, Duration.ofSeconds(15)));
    endpoint2.getTransfers().add(
            TransferViewModel.Receive("1234", TransferViewModel.Result.SUCCESS, "R2D2"));
    endpoint2.getTransfers().add(
            TransferViewModel.Send("1234", TransferViewModel.Result.FAILURE, "R2D2"));
    endpoint2.setState(EndpointViewModel.State.CONNECTED);
    result.addEndpoint(endpoint2);

    return result;
  }
}
