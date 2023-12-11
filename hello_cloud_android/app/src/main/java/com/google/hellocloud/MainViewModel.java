package com.google.hellocloud;

import static com.google.android.gms.nearby.connection.BandwidthInfo.Quality;
import static com.google.hellocloud.Util.TAG;
import static com.google.hellocloud.Util.getDefaultDeviceName;
import static com.google.hellocloud.Util.logErrorAndToast;

import android.content.Context;
import android.util.Log;

import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;

import com.google.android.gms.nearby.Nearby;
import com.google.android.gms.nearby.connection.AdvertisingOptions;
import com.google.android.gms.nearby.connection.BandwidthInfo;
import com.google.android.gms.nearby.connection.ConnectionInfo;
import com.google.android.gms.nearby.connection.ConnectionLifecycleCallback;
import com.google.android.gms.nearby.connection.ConnectionResolution;
import com.google.android.gms.nearby.connection.DiscoveredEndpointInfo;
import com.google.android.gms.nearby.connection.DiscoveryOptions;
import com.google.android.gms.nearby.connection.EndpointDiscoveryCallback;
import com.google.android.gms.nearby.connection.Payload;
import com.google.android.gms.nearby.connection.PayloadCallback;
import com.google.android.gms.nearby.connection.PayloadTransferUpdate;
import com.google.android.gms.nearby.connection.Strategy;

import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Optional;

public final class MainViewModel extends BaseObservable {
  public static MainViewModel shared = createDebugModel();

  public Context context;

  static private final Strategy CURRENT_STRATEGY = Strategy.P2P_CLUSTER;
  static private final String CONNECTION_SERVICE_ID = "com.google.location.nearby.apps.helloconnections";

  private String localEndpointId = "N/A on Android";
  private String localEndpointName = getDefaultDeviceName();
  private final ArrayList<EndpointViewModel> endpoints = new ArrayList<>();

  private boolean isDiscovering;
  private boolean isAdvertising;

  private final ConnectionLifecycleCallback connectionLifecycleCallback = new MyConnectionLifecycleCallback();
  private final EndpointDiscoveryCallback discoveryCallback = new MyDiscoveryCallback();
  private final PayloadCallback payloadCallback = new MyPayloadCallback();

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

  @Bindable
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

  @Bindable
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
    AdvertisingOptions.Builder advertisingOptionsBuilder =
            new AdvertisingOptions.Builder()
                    .setStrategy(CURRENT_STRATEGY)
                    .setLowPower(false);

    Nearby.getConnectionsClient(MainViewModel.shared.context)
            .startAdvertising(
                    localEndpointName,
                    CONNECTION_SERVICE_ID,
                    connectionLifecycleCallback,
                    advertisingOptionsBuilder.build())
            .addOnFailureListener(
                    e -> {
                      isAdvertising = false;
                      notifyPropertyChanged(BR.isAdvertising);
                      logErrorAndToast(
                              MainViewModel.shared.context,
                              R.string.error_toast_cannot_start_advertising,
                              e.getLocalizedMessage());
                    });
  }

  void stopAdvertising() {
    Nearby.getConnectionsClient(MainViewModel.shared.context).stopAdvertising();
  }

  void startDiscovering() {
    DiscoveryOptions.Builder discoveryOptionsBuilder =
            new DiscoveryOptions.Builder()
                    .setStrategy(CURRENT_STRATEGY)
                    .setLowPower(false);

    Nearby.getConnectionsClient(MainViewModel.shared.context)
            .startDiscovery(CONNECTION_SERVICE_ID, discoveryCallback, discoveryOptionsBuilder.build())
            .addOnFailureListener(
                    e -> {
                      isDiscovering = false;
                      notifyPropertyChanged(BR.isDiscovering);
                      logErrorAndToast(
                              MainViewModel.shared.context,
                              R.string.error_toast_cannot_start_discovery,
                              e.getLocalizedMessage());
                    });
  }

  void stopDiscovering() {
    Nearby.getConnectionsClient(MainViewModel.shared.context).stopDiscovery();
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

  class MyConnectionLifecycleCallback extends ConnectionLifecycleCallback {
    private void removeOrChangeState(EndpointViewModel endpoint) {
      // If the endpoint wasn't discovered by us in the first place, remove it.
      // Otherwise, keep it and change its state to Discovered.
      if (endpoint.isIncoming || !isDiscovering) {
        endpoints.remove(endpoint);
      } else {
        endpoint.setState(EndpointViewModel.State.DISCOVERED);
      }
    }

    @Override
    public void onConnectionInitiated(String endpointId, ConnectionInfo info) {
      String endpointName = new String(info.getEndpointInfo(), StandardCharsets.UTF_8);
      Log.v(TAG, String.format("onConnectionInitiated, endpointId: %s, endpointName: %s.",
              endpointId, endpointName));

      Optional<EndpointViewModel> endpoint = getEndpoint(endpointId);

      // Add the endpoint to the endpoint list if necessary.
      if (endpoint.isEmpty()) {
        EndpointViewModel newEndpoint = new EndpointViewModel(endpointId, endpointName);
        addEndpoint(newEndpoint);
        newEndpoint.setState(EndpointViewModel.State.PENDING);
      } else {
        endpoint.get().setState(EndpointViewModel.State.PENDING);
      }

      // Accept automatically.
      Nearby.getConnectionsClient(MainViewModel.shared.context)
              .acceptConnection(endpointId, payloadCallback)
              .addOnFailureListener(e -> logErrorAndToast(context,
                      R.string.error_toast_cannot_accept_connection_request, e.getLocalizedMessage()));
    }

    @Override
    public void onConnectionResult(String endpointId, ConnectionResolution result) {
      Log.v(TAG, String.format("onConnectionResult, endpointId: %s, result: %s",
              endpointId, result.getStatus()));

      Optional<EndpointViewModel> endpoint = getEndpoint(endpointId);
      if (result.getStatus().isSuccess()) {
        endpoint.ifPresent(value -> value.setState(EndpointViewModel.State.CONNECTED));
      } else {
        logErrorAndToast(MainViewModel.shared.context, R.string.error_toast_rejected_by_remote_device,
                endpoint.isPresent() ? endpoint.get().name : endpointId);
        endpoint.ifPresent(value -> value.setState(EndpointViewModel.State.DISCOVERED));
        endpoint.ifPresent(this::removeOrChangeState);
      }
    }

    @Override
    public void onDisconnected(String endpointId) {
      Log.v(TAG, String.format("onDisconnected, endpointId: %s.", endpointId));

      Optional<EndpointViewModel> endpoint = getEndpoint(endpointId);
      endpoint.ifPresent(this::removeOrChangeState);
    }

    @Override
    public void onBandwidthChanged(String endpointId, BandwidthInfo info) {
      String quality;
      switch (info.getQuality()) {
        case Quality.LOW:
          quality = "LOW";
          break;
        case Quality.MEDIUM:
          quality = "MEDIUM";
        case Quality.HIGH:
          quality = "HIGH";
        default:
          quality = "UNKNOWN " + info.getQuality();
      }

      Log.v(TAG, String.format("onBandwidthChanged, quality: %s", quality));
    }
  }

  class MyDiscoveryCallback extends EndpointDiscoveryCallback {
    @Override
    public void onEndpointFound(String endpointId, DiscoveredEndpointInfo info) {
      Log.v(TAG, String.format("onEndpointFound, endpointId: %s", endpointId));

      String endpointName = new String(info.getEndpointInfo(), StandardCharsets.UTF_8);
      Optional<EndpointViewModel> endpoint = getEndpoint(endpointId);
      if (endpoint.isPresent()) {
        endpoint.get().isIncoming = false;
      } else {
        EndpointViewModel newEndpoint = new EndpointViewModel(endpointId, endpointName);
        addEndpoint(newEndpoint);
      }
    }

    @Override
    public void onEndpointLost(String endpointId) {
      Log.v(TAG, String.format("onEndpointLost, endpointId: %s", endpointId));

      Optional<EndpointViewModel> endpoint = getEndpoint(endpointId);
      endpoint.ifPresent(endpoints::remove);
    }
  }

  class MyPayloadCallback extends PayloadCallback {

    @Override
    public void onPayloadReceived(String endpointId, Payload payload) {
      Log.v(TAG, String.format("onPayloadReceived, endpointId: %s", endpointId));

      Optional<EndpointViewModel> endpoint = getEndpoint(endpointId);
      if (endpoint.isEmpty()) {
        Log.v(TAG, String.format("Endpoint not found, endpointId: %s", endpointId));
        return;
      }

      if (endpoint.get().getState() != EndpointViewModel.State.RECEIVING) {
        endpoint.get().setState(EndpointViewModel.State.SENDING);
      }
      // TODO: decode payload into incoming files and add them to endpoint.incomingFiles
    }

    @Override
    public void onPayloadTransferUpdate(String endpointId, PayloadTransferUpdate update) {

    }
  }
}
