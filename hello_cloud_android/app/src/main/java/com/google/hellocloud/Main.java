package com.google.hellocloud;

import static com.google.android.gms.nearby.connection.BandwidthInfo.Quality;
import static com.google.hellocloud.Util.TAG;
import static com.google.hellocloud.Util.getDefaultDeviceName;
import static com.google.hellocloud.Util.logErrorAndToast;

import android.content.Context;
import android.util.Log;
import androidx.annotation.NonNull;
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
import com.google.gson.GsonBuilder;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Optional;

public final class Main extends BaseObservable {
  public static Main shared = Util.createDebugMain();

  public Context context;

  private static final Strategy CURRENT_STRATEGY = Strategy.P2P_CLUSTER;
  private static final String CONNECTION_SERVICE_ID =
      "com.google.location.nearby.apps.helloconnections";

  private String localEndpointId = "N/A on Android";
  private String localEndpointName = getDefaultDeviceName();
  private final ArrayList<Endpoint> endpoints = new ArrayList<>();

  private boolean isDiscovering;
  private boolean isAdvertising;

  public ArrayList<Packet<OutgoingFile>> outgoingPackets = new ArrayList<>();
  public ArrayList<Packet<IncomingFile>> incomingPackets = new ArrayList<>();

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
  public List<Endpoint> getEndpoints() {
    return endpoints;
  }

  public void addEndpoint(Endpoint endpoint) {
    endpoints.add(endpoint);
    notifyPropertyChanged(BR.endpoints);
  }

  public Optional<Endpoint> getEndpoint(String endpointId) {
    return endpoints.stream().filter(e -> Objects.equals(e.id, endpointId)).findFirst();
  }

  void startAdvertising() {
    AdvertisingOptions.Builder advertisingOptionsBuilder =
        new AdvertisingOptions.Builder().setStrategy(CURRENT_STRATEGY).setLowPower(false);

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
        new DiscoveryOptions.Builder().setStrategy(CURRENT_STRATEGY).setLowPower(false);

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
    endpoints.removeIf(p -> p.getState() == Endpoint.State.DISCOVERED);
    notifyPropertyChanged(BR.endpoints);
  }

  void requestConnection(String endpointId) {
    Optional<Endpoint> endpoint = getEndpoint(endpointId);
    endpoint.ifPresentOrElse(
        p -> {
          byte[] info = StandardCharsets.UTF_8.encode(localEndpointName).array();
          Nearby.getConnectionsClient(shared.context)
              .requestConnection(
                  info,
                  endpointId,
                  connectionLifecycleCallback,
                  new ConnectionOptions.Builder().build())
              .addOnFailureListener(
                  e -> {
                    logErrorAndToast(
                        context,
                        R.string.error_toast_cannot_send_connection_request,
                        e.getLocalizedMessage());
                    p.setState(Endpoint.State.DISCOVERED);
                  });
        },
        () -> logErrorAndToast(context, R.string.error_toast_endpoint_lost, endpointId));
  }

  void disconnect(String endpointId) {
    Nearby.getConnectionsClient(shared.context).disconnectFromEndpoint(endpointId);
  }

  private final ConnectionLifecycleCallback connectionLifecycleCallback =
      new ConnectionLifecycleCallback() {
        private void removeOrChangeState(Endpoint endpoint) {
          // If the endpoint wasn't discovered by us in the first place, remove it.
          // Otherwise, keep it but change its state to Discovered.
          endpoint.setState(Endpoint.State.DISCOVERED);
          if (endpoint.isIncoming || !isDiscovering) {
            endpoints.remove(endpoint);
            notifyPropertyChanged(BR.endpoints);
          }
        }

        @Override
        public void onConnectionInitiated(@NonNull String endpointId, ConnectionInfo info) {
          String endpointName = new String(info.getEndpointInfo(), StandardCharsets.UTF_8);
          Log.v(
              TAG,
              String.format(
                  "onConnectionInitiated, endpointId: %s, endpointName: %s.",
                  endpointId, endpointName));

          Optional<Endpoint> endpoint = getEndpoint(endpointId);

          // Add the endpoint to the endpoint list if necessary.
          endpoint.ifPresentOrElse(
              p -> p.setState(Endpoint.State.CONNECTING),
              () -> {
                Endpoint newEndpoint = new Endpoint(endpointId, endpointName, true);
                addEndpoint(newEndpoint);
                newEndpoint.setState(Endpoint.State.CONNECTING);
              });

          // Accept automatically.
          Nearby.getConnectionsClient(shared.context)
              .acceptConnection(endpointId, payloadCallback)
              .addOnFailureListener(
                  e ->
                      logErrorAndToast(
                          context,
                          R.string.error_toast_cannot_accept_connection_request,
                          e.getLocalizedMessage()));
        }

        @Override
        public void onConnectionResult(@NonNull String endpointId, ConnectionResolution result) {
          Log.v(
              TAG,
              String.format(
                  "onConnectionResult, endpointId: %s, result: %s",
                  endpointId, result.getStatus()));

          Optional<Endpoint> endpoint = getEndpoint(endpointId);
          if (result.getStatus().isSuccess()) {
            endpoint.ifPresent(value -> value.setState(Endpoint.State.CONNECTED));
          } else {
            logErrorAndToast(
                shared.context,
                R.string.error_toast_rejected_by_remote_device,
                endpoint.isPresent() ? endpoint.get().name : endpointId);
            endpoint.ifPresent(this::removeOrChangeState);
          }
        }

        @Override
        public void onDisconnected(@NonNull String endpointId) {
          Log.v(TAG, String.format("onDisconnected, endpointId: %s.", endpointId));
          Optional<Endpoint> endpoint = getEndpoint(endpointId);
          endpoint.ifPresent(this::removeOrChangeState);
        }

        @Override
        public void onBandwidthChanged(@NonNull String endpointId, BandwidthInfo info) {
          Log.v(
              TAG,
              String.format(
                  "onBandwidthChanged, endpointId:%s, quality:%d", endpointId, info.getQuality()));

          String qualityDescription;
          int quality = info.getQuality();
          qualityDescription =
              switch (quality) {
                case Quality.LOW -> "LOW";
                case Quality.MEDIUM -> "MEDIUM";
                case Quality.HIGH -> "HIGH";
                default -> "UNKNOWN " + quality;
              };

          Log.v(TAG, String.format("onBandwidthChanged, quality: %s", qualityDescription));
        }
      };

  private final EndpointDiscoveryCallback discoveryCallback =
      new EndpointDiscoveryCallback() {
        @Override
        public void onEndpointFound(@NonNull String endpointId, DiscoveredEndpointInfo info) {
          Log.v(TAG, String.format("onEndpointFound, endpointId: %s", endpointId));

          String endpointName = new String(info.getEndpointInfo(), StandardCharsets.UTF_8);
          Optional<Endpoint> endpoint = getEndpoint(endpointId);
          if (endpoint.isPresent()) {
            endpoint.get().isIncoming = false;
          } else {
            Endpoint newEndpoint = new Endpoint(endpointId, endpointName);
            addEndpoint(newEndpoint);
          }
        }

        @Override
        public void onEndpointLost(@NonNull String endpointId) {
          Log.v(TAG, String.format("onEndpointLost, endpointId: %s", endpointId));

          Optional<Endpoint> endpoint = getEndpoint(endpointId);
          endpoint.ifPresent(endpoints::remove);
        }
      };

  private final PayloadCallback payloadCallback =
      new PayloadCallback() {
        @Override
        public void onPayloadReceived(@NonNull String endpointId, @NonNull Payload payload) {
          Log.v(TAG, String.format("onPayloadReceived, endpointId: %s", endpointId));

          String json = new String(payload.asBytes(), StandardCharsets.UTF_8);
          GsonBuilder gson = new GsonBuilder();
          gson.registerTypeAdapter(DataWrapper.class, new DataWrapper.Deserializer());
          DataWrapper<IncomingFile> data = gson.create().fromJson(json, DataWrapper.class);

          Optional<Endpoint> endpoint = getEndpoint(endpointId);
          endpoint.ifPresent(
              p -> {
                if (p.getState() != Endpoint.State.SENDING) {
                  if (data.kind == DataWrapper.Kind.PACKET) {
                    p.onPacketReceived(data.packet);
                  } else if (data.kind == DataWrapper.Kind.NOTIFICATION_TOKEN){
                    p.onNotificationTokenReceived(data.notificationToken);
                  }
                }
              });
        }

        @Override
        public void onPayloadTransferUpdate(
            @NonNull String endpointId, @NonNull PayloadTransferUpdate update) {
          Log.v(TAG, String.format("onPayloadTransferUpdate, endpointId: %s", endpointId));

          Optional<Endpoint> endpoint = getEndpoint(endpointId);
          endpoint.ifPresent(p -> p.onPacketTransferUpdate(update.getStatus()));
        }
      };
}
