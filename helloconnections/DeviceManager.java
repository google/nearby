package com.google.location.nearby.apps.helloconnections;

import android.content.Context;
import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;
import com.google.android.gms.nearby.connection.ConnectionLifecycleCallback;
import com.google.android.gms.nearby.connection.ConnectionsDevice;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.google.location.nearby.apps.helloconnections.Device.DeviceState;
import java.util.List;
import java.util.Map;
import java.util.Observable;

/** Keeps track of currently active {@link Device} objects. */
class DeviceManager extends Observable {

  private static final DeviceManager instance = new DeviceManager();

  private final Map<String, Device> devices = new ArrayMap<>();

  /** Events that can occur on a device. */
  enum DeviceEvent {
    CONNECTION_STATE_CHANGE,
    PAYLOAD_CHANGE,
  }

  static DeviceManager getInstance() {
    return instance;
  }

  /**
   * Retrieve the current list of active {@link Device} objects.
   *
   * @return the list of active devices
   */
  List<Device> getDevices() {
    return ImmutableList.copyOf(devices.values());
  }

  /**
   * Retrieve a {@link Device} linked to an endpoint ID.
   *
   * @param id the endpoint ID of the requested device
   * @return the device linked to the given endpoint ID, or null if none exists
   */
  @Nullable
  Device getDevice(String id) {
    return devices.get(id);
  }

  /**
   * Creates a new {@link Device} for an endpoint ID.
   *
   * @param context the calling context for which the device was found
   * @param name the endpoint name of the device
   * @param id the endpoint ID of the device
   * @param connectionLifecycleCallback the lifecycle callback for this device
   * @return the newly created device for the given endpoint ID
   */
  Device createDevice(
      Context context,
      String name,
      String id,
      ConnectionLifecycleCallback connectionLifecycleCallback) {
    Device device = new Device(context, name, id, connectionLifecycleCallback);
    devices.put(id, device);
    notifyObserversOfChange(DeviceEvent.CONNECTION_STATE_CHANGE);

    return device;
  }

  /**
   * Creates a new {@link Device} for a PresenceDevice for NC v2 usage.
   *
   * @param context the calling context for which the device was found
   * @param ncDevice the ConnectionsDevice holding the device info
   * @param name name of the device
   * @param callback v2 connection lifecycle callback
   * @return the newly created device
   */
  @SuppressWarnings("GmsCoreFirstPartyApiChecker")
  Device createV2Device(
      Context context,
      ConnectionsDevice ncDevice,
      String name,
      com.google.android.gms.nearby.connection.v2.ConnectionLifecycleCallback<ConnectionsDevice>
          callback) {
    Device device = new Device(context, ncDevice, name, callback);
    devices.put(ncDevice.getEndpointId(), device);
    notifyObserversOfChange(DeviceEvent.CONNECTION_STATE_CHANGE);
    return device;
  }

  /**
   * Removes a {@link Device} for a given endpoint ID
   *
   * @param id the endpoint ID of the device to remove
   */
  void removeDevice(String id) {
    devices.remove(id);
    notifyObserversOfChange(DeviceEvent.CONNECTION_STATE_CHANGE);
  }

  /**
   * Removes all {@link Device} objects that are unconnected. This essentially means all devices in
   * the DISCOVERED state.
   */
  void removeUnconnectedDevices() {
    // Iterate through a copy of our endpoint IDs, so the key set doesn't change while we modify
    // the devices map.
    for (String id : ImmutableSet.copyOf(devices.keySet())) {
      if (DeviceState.DISCOVERED == devices.get(id).getDeviceState()) {
        devices.remove(id);
      }
    }
    notifyObserversOfChange(DeviceEvent.CONNECTION_STATE_CHANGE);
  }

  /** Removes all {@link Device} objects, regardless of their state. */
  void clearDevices() {
    devices.clear();
    notifyObserversOfChange(DeviceEvent.CONNECTION_STATE_CHANGE);
  }

  /**
   * Indicates that a {@link Device} has been added/removed/changed and notifies the observers.
   *
   * @param deviceEvent the type of change that just occurred
   */
  void notifyObserversOfChange(DeviceEvent deviceEvent) {
    setChanged();
    notifyObservers(deviceEvent);
  }
}
