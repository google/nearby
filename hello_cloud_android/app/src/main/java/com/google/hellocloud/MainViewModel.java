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
import com.google.android.gms.nearby.connection.ConnectionOptions;
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
import java.util.Arrays;
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

  public void addEndpoint(EndpointViewModel endpoint) {
    endpoints.add(endpoint);
    notifyPropertyChanged(BR.endpoints);
  }

  public Optional<EndpointViewModel> getEndpoint(String endpointId) {
    return endpoints.stream().filter(e -> Objects.equals(e.id, endpointId)).findFirst();
  }

  void startAdvertising() {
    AdvertisingOptions.Builder advertisingOptionsBuilder =
            new AdvertisingOptions.Builder()
                    .setStrategy(CURRENT_STRATEGY)
                    .setLowPower(false);

    Nearby.getConnectionsClient(shared.context)
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
                              shared.context,
                              R.string.error_toast_cannot_start_advertising,
                              e.getLocalizedMessage());
                    });
  }

  void stopAdvertising() {
    Nearby.getConnectionsClient(shared.context).stopAdvertising();
  }

  void startDiscovering() {
    DiscoveryOptions.Builder discoveryOptionsBuilder =
            new DiscoveryOptions.Builder()
                    .setStrategy(CURRENT_STRATEGY)
                    .setLowPower(false);

    Nearby.getConnectionsClient(shared.context)
            .startDiscovery(CONNECTION_SERVICE_ID, discoveryCallback, discoveryOptionsBuilder.build())
            .addOnFailureListener(
                    e -> {
                      isDiscovering = false;
                      notifyPropertyChanged(BR.isDiscovering);
                      logErrorAndToast(
                              shared.context,
                              R.string.error_toast_cannot_start_discovery,
                              e.getLocalizedMessage());
                    });
  }

  void stopDiscovering() {
    Nearby.getConnectionsClient(shared.context).stopDiscovery();
    endpoints.removeIf(p -> p.getState() != EndpointViewModel.State.DISCOVERED);
  }

  void requestConnection(String endpointId) {
    Optional<EndpointViewModel> endpoint = getEndpoint(endpointId);
    if (endpoint.isPresent()) {
      ConnectionOptions.Builder builder = new ConnectionOptions.Builder();

      byte[] info = StandardCharsets.UTF_8.encode(localEndpointName).array();
      Nearby.getConnectionsClient(shared.context).requestConnection(
                      info, endpointId, connectionLifecycleCallback, builder.build())
              .addOnFailureListener(
                      e -> {
                        logErrorAndToast(
                                context,
                                R.string.error_toast_cannot_send_connection_request,
                                e.getLocalizedMessage());
                        endpoint.get().setState(EndpointViewModel.State.DISCOVERED);
                      });
    } else {
      logErrorAndToast(context, R.string.error_toast_endpoint_lost, endpointId);
      return;
    }
  }

  void disconnect(String endpointId) {
    Nearby.getConnectionsClient(shared.context).disconnectFromEndpoint(endpointId);
  }

  void sendFiles(String endpointId, List<OutgoingFileViewModel> files) {
    String json = OutgoingFileViewModel.encodeOutgoingFiles(files);
    Optional<EndpointViewModel> maybeEndpoint = getEndpoint(endpointId);
    if (maybeEndpoint.isPresent()) {
      assert maybeEndpoint.get().getState() == EndpointViewModel.State.CONNECTED;
      maybeEndpoint.get().setState(EndpointViewModel.State.SENDING);

      Payload payload = Payload.fromBytes(json.getBytes(StandardCharsets.UTF_8));
      Nearby.getConnectionsClient(shared.context).sendPayload(endpointId, payload)
              .addOnFailureListener(e -> {
                logErrorAndToast(shared.context, R.string.error_toast_cannot_send_payload, e);
                maybeEndpoint.get().setState(EndpointViewModel.State.CONNECTED);
              });
    }
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
            "image/jpeg", "IMG_1001.jpeg", "2C84B738-22B9-481A-9D18-8A37FAC0202C.jpeg", 100000, IncomingFileViewModel.State.RECEIVED));
    endpoint2.getIncomingFiles().add(new IncomingFileViewModel(
            "image/jpeg", "IMG_1002.jpeg", "1234", 100000, IncomingFileViewModel.State.DOWNLOADING));
    endpoint2.getIncomingFiles().add(new IncomingFileViewModel(
            "image/jpeg", "IMG_1003.jpeg", "1234", 100000, IncomingFileViewModel.State.DOWNLOADED));

    endpoint2.getTransfers().add(TransferViewModel.download(
            "1234", TransferViewModel.Result.SUCCESS, 100000, Duration.ofSeconds(10)));
    endpoint2.getTransfers().add(TransferViewModel.upload(
            "1234", TransferViewModel.Result.CANCELED, 2000000, Duration.ofSeconds(15)));
    endpoint2.getTransfers().add(
            TransferViewModel.receive("1234", TransferViewModel.Result.SUCCESS, "R2D2"));
    endpoint2.getTransfers().add(
            TransferViewModel.send("1234", TransferViewModel.Result.FAILURE, "R2D2"));
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
        // TODO: tell the activity to navigate back to main fragment.
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
        EndpointViewModel newEndpoint = new EndpointViewModel(endpointId, endpointName, true);
        addEndpoint(newEndpoint);
        newEndpoint.setState(EndpointViewModel.State.PENDING);
      } else {
        endpoint.get().setState(EndpointViewModel.State.PENDING);
      }

      // Accept automatically.
      Nearby.getConnectionsClient(shared.context)
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
        logErrorAndToast(shared.context, R.string.error_toast_rejected_by_remote_device,
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
      String qualityDescription;
      int quality = info.getQuality();
      switch (quality) {
        case Quality.LOW:
          qualityDescription = "LOW";
          break;
        case Quality.MEDIUM:
          qualityDescription = "MEDIUM";
          break;
        case Quality.HIGH:
          qualityDescription = "HIGH";
          break;
        default:
          qualityDescription = "UNKNOWN " + quality;
      }

      Log.v(TAG, String.format("onBandwidthChanged, quality: %s", qualityDescription));
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

      if (endpoint.get().getState() != EndpointViewModel.State.SENDING) {
        endpoint.get().setState(EndpointViewModel.State.RECEIVING);
        String json = new String(payload.asBytes(), StandardCharsets.UTF_8);
        IncomingFileViewModel[] files = IncomingFileViewModel.decodeIncomingFiles(json);
        for (IncomingFileViewModel file : files) {
          // TODO: add a transfer
          file.setState(IncomingFileViewModel.State.RECEIVED);
        }
        endpoint.get().onFilesReceived(Arrays.asList(files));
      }
    }

    @Override
    public void onPayloadTransferUpdate(String endpointId, PayloadTransferUpdate update) {
      Log.v(TAG, String.format("onPayloadTransferUpdate, endpointId: %s", endpointId));
      int status = update.getStatus();
      Optional<EndpointViewModel> endpoint = getEndpoint(endpointId);
      if (status == PayloadTransferUpdate.Status.IN_PROGRESS) {
        return;
      } else if (status == PayloadTransferUpdate.Status.SUCCESS) {
//        result = TransferModel.Result.Success; // add to transfer log
        if (endpoint.isPresent()) {
          boolean isSending = endpoint.get().getState() == EndpointViewModel.State.SENDING;
          endpoint.get().setState(EndpointViewModel.State.CONNECTED);
          if (isSending) {
            endpoint.get().getOutgoingFiles().clear();
          }
        }
      } else {
        // Failed or canceled
        //        result = TransferModel.Result.Failed or Cancelled; // add to transfer log
        endpoint.ifPresent(p -> p.setState(EndpointViewModel.State.CONNECTED));
      }
    }
  }
}
