package com.google.location.nearby.apps.helloconnections;

import static com.google.location.nearby.apps.helloconnections.PayloadUtils.createBytesPayload;
import static com.google.location.nearby.apps.helloconnections.PayloadUtils.createFilePayload;
import static com.google.location.nearby.apps.helloconnections.PayloadUtils.createStreamPayload;
import static com.google.location.nearby.apps.helloconnections.Util.TAG;
import static com.google.location.nearby.apps.helloconnections.Util.logErrorAndToast;

import android.content.Context;
import android.util.Log;
import androidx.annotation.Nullable;
import androidx.collection.SimpleArrayMap;
import com.google.android.gms.nearby.Nearby;
import com.google.android.gms.nearby.connection.BluetoothConnectivityInfo;
import com.google.android.gms.nearby.connection.ConnectionLifecycleCallback;
import com.google.android.gms.nearby.connection.ConnectionOptions;
import com.google.android.gms.nearby.connection.ConnectionResolution;
import com.google.android.gms.nearby.connection.ConnectionsDevice;
import com.google.android.gms.nearby.connection.ConnectionsStatusCodes;
import com.google.android.gms.nearby.connection.ConnectivityInfo;
import com.google.android.gms.nearby.connection.Medium;
import com.google.android.gms.nearby.connection.Payload;
import com.google.android.gms.nearby.connection.PayloadCallback;
import com.google.android.gms.nearby.connection.PayloadTransferUpdate;
import com.google.location.nearby.apps.helloconnections.DeviceManager.DeviceEvent;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** A class to represent a remote Device found by Nearby Connections */
public class Device {

  private final Context context;

  private final String name;
  private final String id;
  @Nullable private String connectedMedium = null;

  private final PayloadTransfers payloadTransfers = new PayloadTransfers();
  @Nullable private final ConnectionLifecycleCallback connectionLifecycleCallback;

  @Nullable
  private final com.google.android.gms.nearby.connection.v2.ConnectionLifecycleCallback<
          ConnectionsDevice>
      v2ConnectionLifecycleCallback;

  @Nullable private final ConnectionsDevice connectionsDevice;

  private DeviceState deviceState;
  private String authToken;

  /** The states for NearbyConnections remote Devices. */
  enum DeviceState {
    DISCOVERED,
    CONNECTION_REQUESTED,
    CONNECTION_INITIATED,
    AWAITING_CONNECTION_RESULT,
    CONNECTED,
  }

  private final PayloadCallback payloadCallback =
      new PayloadCallback() {
        @Override
        public void onPayloadReceived(String endpointId, Payload payload) {
          long payloadId = payload.getId();

          if (payloadTransfers.contains(payloadId)) {
            Log.d(
                TAG, String.format("Payload already exists. Ignored. Payload ID: %d.", payloadId));
            return;
          }

          payloadTransfers.add(PayloadTransfer.createIncomingPayloadTransfer(context, payload));
          DeviceManager.getInstance().notifyObserversOfChange(DeviceEvent.PAYLOAD_CHANGE);
        }

        @Override
        public void onPayloadTransferUpdate(String endpointId, PayloadTransferUpdate update) {
          Log.d(
              TAG,
              String.format(
                  "Payload updated: ID:%d. Status:%d. TotalBytes:%d. BytesTransferred:%d.",
                  update.getPayloadId(),
                  update.getStatus(),
                  update.getTotalBytes(),
                  update.getBytesTransferred()));

          long payloadId = update.getPayloadId();

          if (!payloadTransfers.contains(payloadId)) {
            Log.d(
                TAG,
                String.format(
                    "Received PayloadTransferUpdate of unreceived payload %d.", payloadId));
            return;
          }

          // Update the percentage and the progress bar shown in PayloadTransferDialogFragment.
          payloadTransfers.get(payloadId).notifyUpdate(update);
          DeviceManager.getInstance().notifyObserversOfChange(DeviceEvent.PAYLOAD_CHANGE);
        }
      };

  // TODO(alexanderkang): Make this class parcelable.
  Device(
      Context context,
      String name,
      String id,
      @Nullable ConnectionLifecycleCallback connectionLifecycleCallback) {
    this.context = context;
    this.name = name;
    this.id = id;
    this.connectionLifecycleCallback = connectionLifecycleCallback;
    this.v2ConnectionLifecycleCallback = null;
    connectionsDevice = null;
    deviceState = DeviceState.DISCOVERED;
  }

  @SuppressWarnings("GmsCoreFirstPartyApiChecker")
  Device(
      Context context,
      ConnectionsDevice device,
      String name,
      com.google.android.gms.nearby.connection.v2.ConnectionLifecycleCallback<ConnectionsDevice>
          callback) {
    this.context = context;
    this.id = device.getEndpointId();
    this.name = name;
    this.connectionsDevice = device;
    this.connectionLifecycleCallback = null;
    this.v2ConnectionLifecycleCallback = callback;
    deviceState = DeviceState.DISCOVERED;
  }

  String getAuthToken() {
    return authToken;
  }

  String getName() {
    return name;
  }

  String getId() {
    return id;
  }

  @Nullable
  String getMedium() {
    return connectedMedium;
  }

  void setMedium(String medium) {
    connectedMedium = medium;
    DeviceManager.getInstance().notifyObserversOfChange(DeviceEvent.CONNECTION_STATE_CHANGE);
  }

  DeviceState getDeviceState() {
    return deviceState;
  }

  List<PayloadTransfer> getPayloadTransferList() {
    return payloadTransfers.getList();
  }

  private void setDeviceState(DeviceState deviceState) {
    this.deviceState = deviceState;
    DeviceManager.getInstance().notifyObserversOfChange(DeviceEvent.CONNECTION_STATE_CHANGE);
  }

  @SuppressWarnings({"GmsCoreFirstPartyApiChecker", "AndroidJdkLibsChecker"})
  void connect(String fromEndpointName, List<String> mediumList) {
    Log.v(TAG, "connect()");

    if (connectionLifecycleCallback != null) {
      ConnectionOptions.Builder builder =
          new ConnectionOptions.Builder().setEnforceTopologyConstraints(false);
      int[] mediums = parseMediums(mediumList);
      if (mediums != null && mediums.length > 0) {
        Log.d(TAG, "Enable mediums " + Arrays.toString(mediums));
        builder.setConnectionMediums(mediums);
        builder.setUpgradeMediums(mediums);
      }

      Nearby.getConnectionsClient(context)
          .requestConnection(
              fromEndpointName.getBytes(), id, connectionLifecycleCallback, builder.build())
          .addOnFailureListener(
              e -> {
                logErrorAndToast(
                    context,
                    R.string.error_toast_cannot_send_connection_request,
                    e.getLocalizedMessage());
                setDeviceState(DeviceState.DISCOVERED);
              });
      setDeviceState(DeviceState.CONNECTION_REQUESTED);
    } else if (v2ConnectionLifecycleCallback != null) {
      ConnectivityInfo connectivityInfo =
          getConnectivityInfo(connectionsDevice.getConnectivityInfoList(), Medium.BLUETOOTH);
      byte[] remoteBluetoothMacAddress = null;
      if (connectivityInfo != null) {
        remoteBluetoothMacAddress =
            ((BluetoothConnectivityInfo) connectivityInfo).getBluetoothMacAddress();
      }
      ConnectionOptions.Builder builder =
          new ConnectionOptions.Builder()
              .setConnectionMediums(
                  connectionsDevice.getConnectivityInfoList().stream()
                      .mapToInt(ConnectivityInfo::getMediumType)
                      .toArray())
              .setRemoteBluetoothMacAddress(connectionsDevice.getBluetoothMacAddress());

      Nearby.getConnectionsClient(context)
          .requestConnection(
              MainActivity.CONNECTION_SERVICE_ID,
              connectionsDevice,
              v2ConnectionLifecycleCallback,
              builder.build())
          .addOnFailureListener(
              e -> {
                logErrorAndToast(
                    context,
                    R.string.error_toast_cannot_send_connection_request,
                    e.getLocalizedMessage());
                setDeviceState(DeviceState.DISCOVERED);
              });
      setDeviceState(DeviceState.CONNECTION_REQUESTED);
    } else {
      // should not go here.
    }
  }

  int[] parseMediums(List<String> mediumList) {
    int[] numbers = new int[mediumList.size()];
    int idx = 0;
    for (String medium : mediumList) {
      switch (medium) {
        case "WIFI_LAN":
          numbers[idx++] = Medium.WIFI_LAN;
          break;
        case "WIFI_AWARE":
          numbers[idx++] = Medium.WIFI_AWARE;
          break;
        case "WIFI_DIRECT":
          numbers[idx++] = Medium.WIFI_DIRECT;
          break;
        case "WIFI_HOTSPOT":
          numbers[idx++] = Medium.WIFI_HOTSPOT;
          break;
        case "WEB_RTC":
          numbers[idx++] = Medium.WEB_RTC;
          break;
        case "BLUETOOTH":
          numbers[idx++] = Medium.BLUETOOTH;
          break;
        case "BLE":
          numbers[idx++] = Medium.BLE;
          break;
        case "BLE_L2CAP":
          numbers[idx++] = Medium.BLE_L2CAP;
          break;
        case "NFC":
          numbers[idx++] = Medium.NFC;
          break;
        case "USB":
          numbers[idx++] = Medium.USB;
          break;
        default:
          // Fall through.
      }
    }
    return numbers;
  }

  void notifyConnectionInitiated(String authToken) {
    this.authToken = authToken;
    setDeviceState(DeviceState.CONNECTION_INITIATED);
  }

  void notifyConnectionResult(ConnectionResolution result) {
    if (result.getStatus().isSuccess()) {
      setDeviceState(DeviceState.CONNECTED);
      return;
    }

    if (result.getStatus().getStatusCode() == ConnectionsStatusCodes.STATUS_CONNECTION_REJECTED) {
      // TODO(b/31967172): move the toast out of the Device class (probably use another listener or
      // Observer to handle the toast.
      logErrorAndToast(context, R.string.error_toast_rejected_by_remote_device, name);
    }

    // Whether or not it was a rejection, we still failed to connect. So reset the device state.
    DeviceManager.getInstance().removeDevice(id);
  }

  void notifyDisconnect() {
    DeviceManager.getInstance().removeDevice(id);
  }

  void accept() {
    Log.v(TAG, "accept()");

    Nearby.getConnectionsClient(context)
        .acceptConnection(id, payloadCallback)
        .addOnFailureListener(
            e ->
                logErrorAndToast(
                    context,
                    R.string.error_toast_cannot_accept_connection_request,
                    e.getLocalizedMessage()));
    setDeviceState(DeviceState.AWAITING_CONNECTION_RESULT);
  }

  void reject() {
    Log.v(TAG, "reject()");

    Nearby.getConnectionsClient(context)
        .rejectConnection(id)
        .addOnFailureListener(
            e ->
                logErrorAndToast(
                    context,
                    R.string.error_toast_cannot_reject_connection_request,
                    e.getLocalizedMessage()));
    setDeviceState(DeviceState.AWAITING_CONNECTION_RESULT);
  }

  /**
   * Sends a payload of the specified size and type. The contents of the payload are randomized.
   *
   * @param size the size of the payload contents, in bytes
   * @param type the type of payload to send, as specified in {@link Payload.Type}
   */
  void sendPayload(int size, int type) {
    Payload payload = null;
    switch (type) {
      case Payload.Type.BYTES:
        payload = createBytesPayload(size);
        break;
      case Payload.Type.FILE:
        payload = createFilePayload(context, size);
        break;
      case Payload.Type.STREAM:
        payload = createStreamPayload(context, size);
        break;
      default:
        // Fall through.
    }
    if (payload == null) {
      logErrorAndToast(context, R.string.error_toast_cannot_create_payload, type);
      return;
    }

    // Track the payload by putting it in a PayloadTransfer object, and then send it.
    payloadTransfers.add(PayloadTransfer.createOutgoingPayloadTransfer(context, payload));
    Nearby.getConnectionsClient(context).sendPayload(id, payload);
    DeviceManager.getInstance().notifyObserversOfChange(DeviceEvent.PAYLOAD_CHANGE);
  }

  void disconnect() {
    Log.v(TAG, "disconnect()");
    payloadTransfers.clear();
    Nearby.getConnectionsClient(context).disconnectFromEndpoint(id);
    notifyDisconnect();
  }

  /** Keeps track of the {@link PayloadTransfer} objects for this device. */
  private static class PayloadTransfers {
    // The reasons to maintain both a List (payloadTransferList) and a Map (payloadTransferMap):
    //   1. The List is shared with PayloadTransferAdapter, to serve as the data source for the
    //      payloadTransferListView (the ArrayAdapter's constructor can only take a List but not a
    //      Map).
    //   2. The Map is used to improve the performance for element look-up, which happens frequently
    //      along with the callback onPayloadTransferUpdate().
    private final List<PayloadTransfer> payloadTransferList = new ArrayList<>();
    private final SimpleArrayMap<Long, PayloadTransfer> payloadTransferMap = new SimpleArrayMap<>();

    void add(PayloadTransfer transfer) {
      payloadTransferList.add(0, transfer);
      payloadTransferMap.put(transfer.getPayloadId(), transfer);
    }

    List<PayloadTransfer> getList() {
      return payloadTransferList;
    }

    PayloadTransfer get(long payloadId) {
      return payloadTransferMap.get(payloadId);
    }

    void clear() {
      payloadTransferList.clear();
      payloadTransferMap.clear();
    }

    boolean contains(long payloadId) {
      return payloadTransferMap.containsKey(payloadId);
    }
  }

  @Nullable
  private ConnectivityInfo getConnectivityInfo(
      List<ConnectivityInfo> connectivityInfoList, int medium) {
    for (ConnectivityInfo connectivityInfo : connectivityInfoList) {
      if (connectivityInfo.getMediumType() == medium) {
        return connectivityInfo;
      }
    }
    return null;
  }
}
