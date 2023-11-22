package com.google.location.nearby.apps.helloconnections;

import static androidx.core.content.PermissionChecker.PERMISSION_DENIED;
import static com.google.location.nearby.apps.helloconnections.Util.TAG;
import static com.google.location.nearby.apps.helloconnections.Util.logErrorAndToast;
import static com.google.location.nearby.apps.helloconnections.Util.requestPermissionsIfNeeded;

import android.Manifest.permission;
import android.app.admin.DevicePolicyManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.os.ParcelUuid;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.Spinner;
import android.widget.Switch;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import com.google.android.gms.nearby.Nearby;
import com.google.android.gms.nearby.connection.AdvertisingOptions;
import com.google.android.gms.nearby.connection.BandwidthInfo;
import com.google.android.gms.nearby.connection.BandwidthInfo.Quality;
import com.google.android.gms.nearby.connection.BleConnectivityInfo;
import com.google.android.gms.nearby.connection.BluetoothConnectivityInfo;
import com.google.android.gms.nearby.connection.ConnectionInfo;
import com.google.android.gms.nearby.connection.ConnectionLifecycleCallback;
import com.google.android.gms.nearby.connection.ConnectionListeningOptions;
import com.google.android.gms.nearby.connection.ConnectionResolution;
import com.google.android.gms.nearby.connection.ConnectionsDevice;
import com.google.android.gms.nearby.connection.ConnectivityInfo;
import com.google.android.gms.nearby.connection.Device.InstanceType;
import com.google.android.gms.nearby.connection.DiscoveredEndpointInfo;
import com.google.android.gms.nearby.connection.DiscoveryOptions;
import com.google.android.gms.nearby.connection.EndpointDiscoveryCallback;
import com.google.android.gms.nearby.connection.Medium;
import com.google.android.gms.nearby.connection.Strategy;
import com.google.common.collect.ImmutableList;
import com.google.common.primitives.Ints;
import com.google.location.nearby.apps.helloconnections.Device.DeviceState;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** The main (and only) activity of this demo app. */
public class MainActivity extends AppCompatActivity {
  private static final int REQUIRE_PERMISSIONS_FOR_APP_REQUEST_CODE = 0;

  private static final String DEFAULT_NAME = "unknown_hc_target";

  /** Presence advertisement service data uuid, use it for HelloConnection ble advertise usage. */
  public static final ParcelUuid PRESENCE_SERVICE_DATA_UUID =
      ParcelUuid.fromString("0000fcf1-0000-1000-8000-00805f9b34fb");

  private static final ParcelUuid FAST_ADVERTISEMENT_SERVICE_UUID =
      ParcelUuid.fromString("0000FEF3-0000-1000-8000-00805F9B34FB");
  private static final Strategy DEFAULT_STRATEGY = Strategy.P2P_POINT_TO_POINT;
  static final String CONNECTION_SERVICE_ID = "com.google.location.nearby.apps.helloconnections";
  private static final String[] REQUIRED_PERMISSIONS_FOR_APP;

  static {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
      REQUIRED_PERMISSIONS_FOR_APP =
          new String[] {
            permission.READ_CONTACTS,
            permission.BLUETOOTH_SCAN,
            permission.BLUETOOTH_ADVERTISE,
            permission.BLUETOOTH_CONNECT,
            permission.ACCESS_WIFI_STATE,
            permission.CHANGE_WIFI_STATE,
            permission.NEARBY_WIFI_DEVICES,
          };
    } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
      REQUIRED_PERMISSIONS_FOR_APP =
          new String[] {
            permission.READ_CONTACTS,
            permission.BLUETOOTH_SCAN,
            permission.BLUETOOTH_ADVERTISE,
            permission.BLUETOOTH_CONNECT,
            permission.ACCESS_WIFI_STATE,
            permission.CHANGE_WIFI_STATE,
            permission.ACCESS_COARSE_LOCATION,
            permission.ACCESS_FINE_LOCATION,
          };
    } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
      REQUIRED_PERMISSIONS_FOR_APP =
          new String[] {
            permission.READ_CONTACTS,
            permission.BLUETOOTH,
            permission.BLUETOOTH_ADMIN,
            permission.ACCESS_WIFI_STATE,
            permission.CHANGE_WIFI_STATE,
            permission.ACCESS_COARSE_LOCATION,
            permission.ACCESS_FINE_LOCATION,
          };
    } else {
      REQUIRED_PERMISSIONS_FOR_APP =
          new String[] {
            permission.READ_CONTACTS,
            permission.BLUETOOTH,
            permission.BLUETOOTH_ADMIN,
            permission.ACCESS_WIFI_STATE,
            permission.CHANGE_WIFI_STATE,
            permission.ACCESS_COARSE_LOCATION,
          };
    }
  }

  private final boolean[] advertisingOptions = {
    true /* Auto-Upgrade Bandwidth */,
    true /* Enforce Topology Constraints */,
    true /* Enable Bluetooth */,
    true /* Enable BLE */,
    false /* Use Low Power Mode */,
    false /* Use Fast Advertisements */,
    true /* Enable WiFi LAN */,
    true /* Enable NFC */,
    true /* Enable WiFi Aware */,
    true /* Enable WebRTC */,
    true /* Enable USB */,
    false /* Out of band BT */,
    false /* Out of band BLE */
  };
  private final boolean[] discoveryOptions = {
    false /* Forward Unrecognized Bluetooth Devices */,
    true /* Enable Bluetooth */,
    true /* Enable BLE */,
    false /* Use Low Power Mode */,
    false /* Discover Fast Advertisements */,
    true /* Enable WiFi LAN */,
    true /* Enable NFC */,
    true /* Enable WiFi Aware */,
    true /* Enable USB */,
    false /* Out of band BT */,
    false /* Out of band BLE */,
  };

  // Switches that start or stop advertising/discovery.
  private Switch advertiseSwitch;
  private Switch discoverSwitch;

  // Text field to specify local endpoint name.
  private EditText endpointNameField;

  // Current strategy for advertising/discovery
  private Strategy currentStrategy = DEFAULT_STRATEGY;

  private boolean isBtAdvertising = false;
  private boolean isBtScanning = false;
  private boolean isBleAdvertising = false;
  private boolean isBleScanning = false;

  private final ScanCallback scanCallback =
      new ScanCallback() {
        @Override
        @SuppressWarnings("GmsCoreFirstPartyApiChecker")
        public void onScanResult(int callbackType, ScanResult result) {
          ScanRecord scanRecord = result.getScanRecord();
          if (scanRecord == null) {
            return;
          }

          Advertisement advertisement =
              Advertisement.fromBytes(scanRecord.getServiceData(PRESENCE_SERVICE_DATA_UUID));
          if (advertisement == null) {
            return;
          }

          Device device = DeviceManager.getInstance().getDevice(advertisement.endpointId());
          BleConnectivityInfo.Builder connectivityInfo =
              new BleConnectivityInfo.Builder()
                  .setBleMacAddress(Util.macAddressToBytes(result.getDevice().getAddress()));

          byte[] deviceToken = advertisement.deviceToken();
          if (deviceToken != null) {
            connectivityInfo.setDeviceToken(deviceToken);
          }

          byte[] psm = advertisement.psm();
          if (psm != null) {
            connectivityInfo.setPsm(psm);
          }

          if (device == null) {
            ConnectionsDevice connectionsDevice =
                new ConnectionsDevice.Builder()
                    .setEndpointId(advertisement.endpointId())
                    .addConnectivityInfo(connectivityInfo.build())
                    .setInstanceType(advertisement.instanceType())
                    .build();
            var unused =
                DeviceManager.getInstance()
                    .createV2Device(
                        getApplicationContext(),
                        connectionsDevice,
                        advertisement.name(),
                        v2ConnectionLifecycleCallback);
          }
        }

        @Override
        public void onScanFailed(int errorCode) {
          Log.w(TAG, "Ble scan failed to start with error : " + errorCode);
        }
      };

  private final AdvertiseCallback advertiseCallback =
      new AdvertiseCallback() {
        @Override
        public void onStartSuccess(AdvertiseSettings settingsInEffect) {
          Log.w(TAG, "Ble advertising started.");
        }

        @Override
        public void onStartFailure(int errorCode) {
          Log.w(TAG, "Ble advertising failed with code : " + errorCode);
        }
      };

  private final BroadcastReceiver btDiscoveryReceiver =
      new BroadcastReceiver() {
        @Override
        @SuppressWarnings("GmsCoreFirstPartyApiChecker")
        public void onReceive(Context context, Intent intent) {
          String action = intent.getAction();
          if (!BluetoothDevice.ACTION_FOUND.equals(action)) {
            return;
          }

          BluetoothDevice bluetoothDevice = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
          if (bluetoothDevice == null) {
            return;
          }

          String name = bluetoothDevice.getName();
          if (TextUtils.isEmpty(name)) {
            return;
          }

          byte[] data = name.getBytes(StandardCharsets.UTF_8);
          Advertisement advertisement = Advertisement.fromBytes(data);
          if (advertisement == null) {
            return;
          }

          Device device = DeviceManager.getInstance().getDevice(advertisement.endpointId());
          BluetoothConnectivityInfo connectivityInfo =
              new BluetoothConnectivityInfo.Builder()
                  .setBluetoothMacAddress(Util.macAddressToBytes(bluetoothDevice.getAddress()))
                  .build();
          if (device == null) {
            ConnectionsDevice connectionsDevice =
                new ConnectionsDevice.Builder()
                    .setEndpointId(advertisement.endpointId())
                    .setBluetoothMacAddress(Util.macAddressToBytes(bluetoothDevice.getAddress()))
                    .addConnectivityInfo(connectivityInfo)
                    .setEndpointInfo(new byte[0])
                    .setInstanceType(advertisement.instanceType())
                    .build();
            var unused =
                DeviceManager.getInstance()
                    .createV2Device(
                        getApplicationContext(),
                        connectionsDevice,
                        advertisement.name(),
                        v2ConnectionLifecycleCallback);
          }
        }
      };

  private final com.google.android.gms.nearby.connection.v2.ConnectionLifecycleCallback<
          ConnectionsDevice>
      v2ConnectionLifecycleCallback =
          new com.google.android.gms.nearby.connection.v2.ConnectionLifecycleCallback<>() {
            @Override
            @SuppressWarnings("GmsCoreFirstPartyApiChecker")
            public void onConnectionInitiated(
                @NonNull ConnectionsDevice ncDevice, @NonNull ConnectionInfo connectionInfo) {
              Log.v(TAG, "onConnectionInitiated()");
              // Retrieve the pre-existing device, or create a new one if needed.
              Device device = DeviceManager.getInstance().getDevice(ncDevice.getEndpointId());
              if (device == null) {
                device =
                    DeviceManager.getInstance()
                        .createV2Device(getApplicationContext(), ncDevice, DEFAULT_NAME, this);
              }

              device.notifyConnectionInitiated(connectionInfo.getAuthenticationDigits());
            }

            @Override
            @SuppressWarnings("GmsCoreFirstPartyApiChecker")
            public void onConnectionResult(
                @NonNull ConnectionsDevice ncDevice, @NonNull ConnectionResolution resolution) {
              Log.v(TAG, "onConnectionResult()");
              Device device = DeviceManager.getInstance().getDevice(ncDevice.getEndpointId());
              if (device != null) {
                device.notifyConnectionResult(resolution);
              }
            }

            @Override
            public void onBandwidthChanged(
                @NonNull ConnectionsDevice ncDevice, @NonNull BandwidthInfo bandwidthInfo) {
              Log.v(TAG, "onBandwidthChanged");
            }

            @Override
            @SuppressWarnings("GmsCoreFirstPartyApiChecker")
            public void onDisconnected(@NonNull ConnectionsDevice ncDevice) {
              Log.v(TAG, "onDisconnected()");
              Device device = DeviceManager.getInstance().getDevice(ncDevice.getEndpointId());
              if (device != null) {
                device.notifyDisconnect();
              }
            }
          };

  private final ConnectionLifecycleCallback connectionLifecycleCallback =
      new ConnectionLifecycleCallback() {

        @Override
        public void onConnectionInitiated(String endpointId, ConnectionInfo connectionInfo) {
          Log.v(TAG, "onConnectionInitiated()");

          // Retrieve the pre-existing device, or create a new one if needed.
          Device device = DeviceManager.getInstance().getDevice(endpointId);
          if (device == null) {
            device =
                DeviceManager.getInstance()
                    .createDevice(
                        getApplicationContext(),
                        connectionInfo.getEndpointName(),
                        endpointId,
                        this);
          }

          device.notifyConnectionInitiated(connectionInfo.getAuthenticationToken());
        }

        @Override
        public void onConnectionResult(String endpointId, ConnectionResolution result) {
          Log.v(TAG, "onConnectionResult()");

          Device device = DeviceManager.getInstance().getDevice(endpointId);
          if (device != null) {
            device.notifyConnectionResult(result);
          }
        }

        @Override
        public void onDisconnected(String endpointId) {
          Log.v(TAG, "onDisconnected()");

          Device device = DeviceManager.getInstance().getDevice(endpointId);
          if (device != null) {
            device.notifyDisconnect();
          }
        }

        @Override
        public void onBandwidthChanged(String endpointId, BandwidthInfo bandwidthInfo) {
          Log.v(
              TAG,
              "onBandwidthChanged() quality: "
                  + convertQualityToString(bandwidthInfo.getQuality())
                  + ", medium: "
                  + convertMediumToString(bandwidthInfo.getMedium()));

          Device device = DeviceManager.getInstance().getDevice(endpointId);
          if (device != null) {
            device.setMedium(convertMediumToString(bandwidthInfo.getMedium()));
          }
        }

        private String convertQualityToString(int quality) {
          switch (quality) {
            case Quality.LOW:
              return "LOW";
            case Quality.MEDIUM:
              return "MEDIUM";
            case Quality.HIGH:
              return "HIGH";
            default:
              // Fall through
          }

          return "UNKNOWN(" + quality + ")";
        }

        private String convertMediumToString(int medium) {
          switch (medium) {
            case Medium.BLUETOOTH:
              return "BLUETOOTH";
            case Medium.BLE:
              return "BLE";
            case Medium.BLE_L2CAP:
              return "BLE_L2CAP";
            case Medium.WIFI_LAN:
              return "WIFI_LAN";
            case Medium.WIFI_HOTSPOT:
              return "WIFI_HOTSPOT";
            case Medium.WIFI_AWARE:
              return "WIFI_AWARE";
            case Medium.WIFI_DIRECT:
              return "WIFI_DIRECT";
            case Medium.NFC:
              return "NFC";
            case Medium.WEB_RTC:
              return "WEB_RTC";
            case Medium.USB:
              return "USB";
            default:
              // Fall through
          }
          return "UNKNOWN_MEDIUM(" + medium + ")";
        }
      };

  private final EndpointDiscoveryCallback endpointDiscoveryCallback =
      new EndpointDiscoveryCallback() {
        @Override
        public void onEndpointFound(String endpointId, DiscoveredEndpointInfo info) {
          Log.v(TAG, "onEndpointFound()");

          Device device = DeviceManager.getInstance().getDevice(endpointId);
          if (device == null) {
            DeviceManager.getInstance()
                .createDevice(
                    getApplicationContext(),
                    info.getEndpointName(),
                    endpointId,
                    connectionLifecycleCallback);
          }
        }

        @Override
        public void onEndpointLost(String endpointId) {
          Log.d(TAG, "onEndpointLost()");

          Device device = DeviceManager.getInstance().getDevice(endpointId);
          if (device != null && device.getDeviceState() == DeviceState.DISCOVERED) {
            DeviceManager.getInstance().removeDevice(endpointId);
          }
        }
      };

  @SuppressWarnings({"GmsCoreFirstPartyApiChecker", "RestrictedApi"})
  private void startAdvertising() {
    AdvertisingOptions.Builder advertisingOptionsBuilder =
        new AdvertisingOptions.Builder()
            .setStrategy(currentStrategy)
            .setAutoUpgradeBandwidth(advertisingOptions[0])
            .setEnforceTopologyConstraints(advertisingOptions[1])
            .setLowPower(advertisingOptions[4]);
    List<Integer> mediums = new ArrayList<>();
    if (advertisingOptions[2]) {
      mediums.add(Medium.BLUETOOTH);
    }
    if (advertisingOptions[3]) {
      mediums.add(Medium.BLE);
    }
    if (advertisingOptions[6]) {
      mediums.add(Medium.WIFI_LAN);
    }
    if (advertisingOptions[7]) {
      mediums.add(Medium.NFC);
    }
    if (advertisingOptions[8]) {
      mediums.add(Medium.WIFI_AWARE);
    }
    if (advertisingOptions[9]) {
      mediums.add(Medium.WEB_RTC);
    }
    if (advertisingOptions[10]) {
      mediums.add(Medium.USB);
    }
    advertisingOptionsBuilder.setAdvertisingMediums(Ints.toArray(mediums));
    if (advertisingOptions[5] /* Use Fast Advertisements */) {
      advertisingOptionsBuilder.setFastAdvertisementServiceUuid(FAST_ADVERTISEMENT_SERVICE_UUID);
    }

    if (advertisingOptions[11]) {
      startOutOfBandBtListening();
      isBtAdvertising = true;
    } else if (advertisingOptions[12]) {
      if (Build.VERSION.SDK_INT >= VERSION_CODES.LOLLIPOP) {
        startOutOfBandBleListening();
        isBleAdvertising = true;
      } else {
        logErrorAndToast(MainActivity.this, R.string.error_toast_out_of_band_ble_not_supported);
        advertiseSwitch.setChecked(false);
      }
    } else {
      Nearby.getConnectionsClient(this)
          .startAdvertising(
              endpointNameField.getText().toString(),
              CONNECTION_SERVICE_ID,
              connectionLifecycleCallback,
              advertisingOptionsBuilder.build())
          .addOnFailureListener(
              e -> {
                advertiseSwitch.setChecked(false);
                logErrorAndToast(
                    MainActivity.this,
                    R.string.error_toast_cannot_start_advertising,
                    e.getLocalizedMessage());
              });
    }
  }

  @SuppressWarnings("GmsCoreFirstPartyApiChecker")
  @RequiresApi(VERSION_CODES.LOLLIPOP)
  private void startOutOfBandBleListening() {
    ConnectionListeningOptions options =
        new ConnectionListeningOptions.Builder()
            .setStrategy(Strategy.P2P_POINT_TO_POINT)
            .setListeningMediums(new int[] {Medium.BLE})
            .setAutoUpgradeBandwidth(false)
            .build();
    Nearby.getConnectionsClient(this)
        .startListeningForIncomingConnections(
            CONNECTION_SERVICE_ID, v2ConnectionLifecycleCallback, options)
        .addOnFailureListener(
            e ->
                Log.w(
                    TAG,
                    "Failed to start listening for incoming connection, error = "
                        + e.getLocalizedMessage()))
        .addOnSuccessListener(
            listeningResult -> {
              Advertisement.Builder advertisement =
                  Advertisement.builder()
                      .setName(endpointNameField.getText().toString())
                      .setEndpointId(listeningResult.getEndpointId())
                      .setPsm(getPsm(listeningResult.getConnectivityInfoList()))
                      .setDeviceToken(getDeviceToken(listeningResult.getConnectivityInfoList()))
                      .setInstanceType(
                          isWorkProfile() ? InstanceType.SECONDARY : InstanceType.MAIN);
              startBleAdvertising(advertisement.build());
              Log.d(
                  TAG,
                  String.format(
                      "Start listening for incoming ble connection: %s, endpointId: %s",
                      listeningResult.getConnectivityInfoList(), listeningResult.getEndpointId()));
            });
  }

  @Nullable
  @SuppressWarnings("GmsCoreFirstPartyApiChecker")
  private byte[] getPsm(List<ConnectivityInfo> connectivityInfos) {
    for (ConnectivityInfo info : connectivityInfos) {
      if (info.getMediumType() != Medium.BLE) {
        continue;
      }
      return ((BleConnectivityInfo) info).getPsm();
    }
    return null;
  }

  @Nullable
  @SuppressWarnings("GmsCoreFirstPartyApiChecker")
  private byte[] getDeviceToken(List<ConnectivityInfo> connectivityInfos) {
    for (ConnectivityInfo info : connectivityInfos) {
      if (info.getMediumType() != Medium.BLE) {
        continue;
      }
      return ((BleConnectivityInfo) info).getDeviceToken();
    }
    return null;
  }

  @RequiresApi(VERSION_CODES.LOLLIPOP)
  private void startBleAdvertising(Advertisement advertisement) {
    BluetoothLeAdvertiser advertiser =
        BluetoothAdapter.getDefaultAdapter().getBluetoothLeAdvertiser();
    AdvertiseSettings settings =
        new AdvertiseSettings.Builder()
            .setConnectable(true)
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .build();
    AdvertiseData advertiseData =
        new AdvertiseData.Builder().addServiceUuid(PRESENCE_SERVICE_DATA_UUID).build();
    AdvertiseData scanResponse =
        new AdvertiseData.Builder()
            .addServiceData(PRESENCE_SERVICE_DATA_UUID, advertisement.toBleBytes())
            .build();
    advertiser.startAdvertising(settings, advertiseData, scanResponse, advertiseCallback);
  }

  @RequiresApi(VERSION_CODES.LOLLIPOP)
  private void stopBleAdvertising() {
    BluetoothLeAdvertiser advertiser =
        BluetoothAdapter.getDefaultAdapter().getBluetoothLeAdvertiser();
    advertiser.stopAdvertising(advertiseCallback);
  }

  @SuppressWarnings("GmsCoreFirstPartyApiChecker")
  private void startOutOfBandBtListening() {
    ConnectionListeningOptions options =
        new ConnectionListeningOptions.Builder()
            .setStrategy(Strategy.P2P_POINT_TO_POINT)
            .setListeningMediums(new int[] {Medium.BLUETOOTH})
            .setAutoUpgradeBandwidth(false)
            .build();
    Nearby.getConnectionsClient(this)
        .startListeningForIncomingConnections(
            CONNECTION_SERVICE_ID, v2ConnectionLifecycleCallback, options)
        .addOnFailureListener(
            e ->
                Log.w(
                    TAG,
                    "Failed to start listening for incoming connection, error = "
                        + e.getLocalizedMessage()))
        .addOnSuccessListener(
            listeningResult -> {
              Log.d(
                  TAG,
                  String.format(
                      "Start listening for incoming bt connection: %s, endpintId: %s",
                      listeningResult.getConnectivityInfoList(), listeningResult.getEndpointId()));
              Advertisement advertisement =
                  Advertisement.builder()
                      .setName(endpointNameField.getText().toString())
                      .setEndpointId(listeningResult.getEndpointId())
                      .setInstanceType(isWorkProfile() ? InstanceType.SECONDARY : InstanceType.MAIN)
                      .build();
              String name = new String(advertisement.toBtBytes(), StandardCharsets.UTF_8);
              BluetoothAdapter.getDefaultAdapter().setName(name);
              makeBtDiscoverable();
            });
  }

  @SuppressWarnings("GmsCoreFirstPartyApiChecker")
  private void stopAdvertising() {
    if (isBtAdvertising || isBleAdvertising) {
      Nearby.getConnectionsClient(this).stopListeningForIncomingConnections();
      if (isBtAdvertising) {
        BluetoothAdapter.getDefaultAdapter().setName(endpointNameField.getText().toString());
        isBtAdvertising = false;
      }
      if (isBleAdvertising) {
        if (Build.VERSION.SDK_INT >= VERSION_CODES.LOLLIPOP) {
          stopBleAdvertising();
          return;
        }
        isBleAdvertising = false;
      }
    }
    Nearby.getConnectionsClient(this).stopAdvertising();
  }

  @SuppressWarnings("RestrictedApi")
  private void startDiscovery() {
    // Clear any unconnected devices from previous sessions.
    DeviceManager.getInstance().removeUnconnectedDevices();

    DiscoveryOptions.Builder discoveryOptionsBuilder =
        new DiscoveryOptions.Builder()
            .setStrategy(currentStrategy)
            .setForwardUnrecognizedBluetoothDevices(discoveryOptions[0])
            .setLowPower(discoveryOptions[3]);
    List<Integer> mediums = new ArrayList<>();
    if (discoveryOptions[1]) {
      mediums.add(Medium.BLUETOOTH);
    }
    if (discoveryOptions[2]) {
      mediums.add(Medium.BLE);
    }
    if (discoveryOptions[5]) {
      mediums.add(Medium.WIFI_LAN);
    }
    if (discoveryOptions[6]) {
      mediums.add(Medium.NFC);
    }
    if (discoveryOptions[7]) {
      mediums.add(Medium.WIFI_AWARE);
    }
    if (discoveryOptions[8]) {
      mediums.add(Medium.USB);
    }
    discoveryOptionsBuilder.setDiscoveryMediums(Ints.toArray(mediums));
    if (discoveryOptions[4] /* Discover Fast Advertisements */) {
      discoveryOptionsBuilder.setFastAdvertisementServiceUuid(FAST_ADVERTISEMENT_SERVICE_UUID);
    }

    if (discoveryOptions[9]) {
      startBluetoothDiscover();
      isBtScanning = true;
    } else if (discoveryOptions[10]) {
      if (Build.VERSION.SDK_INT >= VERSION_CODES.LOLLIPOP) {
        startBleScanning();
        isBleScanning = true;
      } else {
        logErrorAndToast(MainActivity.this, R.string.error_toast_out_of_band_ble_not_supported);
        discoverSwitch.setChecked(false);
      }
    } else {
      Nearby.getConnectionsClient(this)
          .startDiscovery(
              CONNECTION_SERVICE_ID, endpointDiscoveryCallback, discoveryOptionsBuilder.build())
          .addOnFailureListener(
              e -> {
                discoverSwitch.setChecked(false);
                logErrorAndToast(
                    MainActivity.this,
                    R.string.error_toast_cannot_start_discovery,
                    e.getLocalizedMessage());
              });
    }
  }

  @RequiresApi(VERSION_CODES.LOLLIPOP)
  private void startBleScanning() {
    BluetoothLeScanner scanner = BluetoothAdapter.getDefaultAdapter().getBluetoothLeScanner();
    ScanFilter filter = new ScanFilter.Builder().setServiceUuid(PRESENCE_SERVICE_DATA_UUID).build();
    ScanSettings settings =
        new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
    scanner.startScan(ImmutableList.of(filter), settings, scanCallback);
  }

  @RequiresApi(VERSION_CODES.LOLLIPOP)
  private void stopBleScanning() {
    BluetoothLeScanner scanner = BluetoothAdapter.getDefaultAdapter().getBluetoothLeScanner();
    scanner.stopScan(scanCallback);
  }

  private void startBluetoothDiscover() {
    getApplication()
        .registerReceiver(btDiscoveryReceiver, new IntentFilter(BluetoothDevice.ACTION_FOUND));
    BluetoothAdapter.getDefaultAdapter().startDiscovery();
  }

  private void stopDiscovery() {
    Nearby.getConnectionsClient(this).stopDiscovery();
    if (isBtScanning) {
      getApplication().unregisterReceiver(btDiscoveryReceiver);
      BluetoothAdapter.getDefaultAdapter().cancelDiscovery();
      isBtScanning = false;
    }
    if (isBleScanning) {
      if (Build.VERSION.SDK_INT >= VERSION_CODES.LOLLIPOP) {
        stopBleScanning();
      }
      isBleScanning = false;
    }
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    requestPermissionsIfNeeded(
        this, REQUIRED_PERMISSIONS_FOR_APP, REQUIRE_PERMISSIONS_FOR_APP_REQUEST_CODE);

    setContentView(R.layout.activity_main);

    Spinner strategySpinner = findViewById(R.id.strategySpinner);
    advertiseSwitch = findViewById(R.id.advertiseSwitch);
    ImageButton advertiseOptionsButton = findViewById(R.id.advertisingOptions);
    endpointNameField = findViewById(R.id.endpointName);
    discoverSwitch = findViewById(R.id.discoverSwitch);
    ImageButton discoveryOptionsButton = findViewById(R.id.discoveryOptions);
    RecyclerView devicesRecyclerView = findViewById(R.id.devices);

    // Set the default advertised endpoint name.
    String defaultName = Util.getDefaultDeviceName(this);
    endpointNameField.setText(defaultName);
    endpointNameField.setSelection(defaultName.length());

    strategySpinner.setOnItemSelectedListener(
        new OnItemSelectedListener() {
          @Override
          public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            switch (pos) {
              case 0:
                Log.d(TAG, "P2P_POINT_TO_POINT is selected");
                currentStrategy = Strategy.P2P_POINT_TO_POINT;
                break;
              case 1:
                Log.d(TAG, "P2P_CLUSTER is selected");
                currentStrategy = Strategy.P2P_CLUSTER;
                break;
              case 2:
                Log.d(TAG, "P2P_STAR is selected");
                currentStrategy = Strategy.P2P_STAR;
                break;
              default: // fall out
            }
          }

          @Override
          public void onNothingSelected(AdapterView<?> parent) {}
        });
    advertiseOptionsButton.setOnClickListener(
        v ->
            new AlertDialog.Builder(this)
                .setTitle(R.string.label_advertising_options)
                .setMultiChoiceItems(
                    R.array.advertising_options_array,
                    advertisingOptions,
                    (dialog, which, isChecked) -> advertisingOptions[which] = isChecked)
                .setNeutralButton(R.string.button_done, null)
                .create()
                .show());
    discoveryOptionsButton.setOnClickListener(
        v ->
            new AlertDialog.Builder(this)
                .setTitle(R.string.label_discovery_options)
                .setMultiChoiceItems(
                    R.array.discovery_options_array,
                    discoveryOptions,
                    (dialog, which, isChecked) -> discoveryOptions[which] = isChecked)
                .setNeutralButton(R.string.button_done, null)
                .create()
                .show());
    advertiseSwitch.setOnCheckedChangeListener(
        (buttonView, isChecked) -> {
          if (isChecked) {
            startAdvertising();
          } else {
            stopAdvertising();
          }
        });
    discoverSwitch.setOnCheckedChangeListener(
        (buttonView, isChecked) -> {
          if (isChecked) {
            startDiscovery();
          } else {
            stopDiscovery();
          }
        });

    // Initialize the device list.
    DeviceAdapter deviceAdapter =
        new DeviceAdapter(getSupportFragmentManager(), endpointNameField, getLayoutInflater());
    devicesRecyclerView.setHasFixedSize(true);
    devicesRecyclerView.setLayoutManager(new LinearLayoutManager(this));
    devicesRecyclerView.setItemAnimator(null);
    devicesRecyclerView.setAdapter(deviceAdapter);
  }

  @Override
  public void onStop() {
    Nearby.getConnectionsClient(this).stopAllEndpoints();

    // Reset the UI.
    advertiseSwitch.setChecked(false);
    discoverSwitch.setChecked(false);
    DeviceManager.getInstance().clearDevices();

    super.onStop();
  }

  @Override
  public void onRequestPermissionsResult(
      int requestCode, String[] permissions, int[] grantResults) {
    if (REQUIRE_PERMISSIONS_FOR_APP_REQUEST_CODE == requestCode) {
      if (!allPermissionsGranted(permissions, grantResults)) {
        buildSimpleAlertDialog(getString(R.string.message_permission_not_granted)).show();
      }
    } else {
      super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }
  }

  private boolean allPermissionsGranted(String[] permissions, int[] grantResults) {
    if (permissions.length != grantResults.length) {
      Log.d(
          TAG,
          "Invalid permission request result, request "
              + permissions.length
              + " permissions but got "
              + grantResults.length
              + " results");
    }

    boolean success = true;
    for (int i = 0; i < grantResults.length; i++) {
      if (grantResults[i] == PERMISSION_DENIED) {
        Log.d(TAG, "Failed to grant " + permissions[i]);
        success = false;
      }
    }
    return success;
  }

  private AlertDialog buildSimpleAlertDialog(String message) {
    return new AlertDialog.Builder(this)
        .setMessage(message)
        .setNegativeButton(getString(R.string.button_done), (dialog, which) -> finish())
        .setCancelable(false)
        .create();
  }

  private void makeBtDiscoverable() {
    int requestCode = 1;
    Intent discoverableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
    startActivityForResult(discoverableIntent, requestCode);
  }

  public boolean isWorkProfile() {
    DevicePolicyManager devicePolicyManager =
        (DevicePolicyManager) getSystemService(Context.DEVICE_POLICY_SERVICE);
    List<ComponentName> activeAdmins = devicePolicyManager.getActiveAdmins();
    if (activeAdmins != null) {
      for (ComponentName admin : activeAdmins) {
        String packageName = admin.getPackageName();
        if (devicePolicyManager.isProfileOwnerApp(packageName)) {
          return true;
        }
      }
    }
    return false;
  }
}
