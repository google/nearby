package com.google.location.nearby.apps.helloconnections;

import android.os.Bundle;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AlertDialog;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;
import androidx.annotation.CallSuper;
import androidx.annotation.IntDef;
import androidx.annotation.LayoutRes;
import com.google.location.nearby.apps.helloconnections.DeviceAdapter.DeviceViewHolder;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Locale;

/** An adapter to manage a list of remote {@link Device} objects. */
class DeviceAdapter extends RecyclerView.Adapter<DeviceViewHolder> {

  private static final String TAG_TRANSFER_DIALOG = "Transfer Dialog";

  /** The various list items within this adapter. */
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({
    ViewType.DISCOVERED,
    ViewType.CONNECTION_REQUESTED,
    ViewType.CONNECTION_INITIATED,
    ViewType.AWAITING_CONNECTION_RESULT,
    ViewType.CONNECTED
  })
  private @interface ViewType {
    int DISCOVERED = 0;
    int CONNECTION_REQUESTED = 1;
    int CONNECTION_INITIATED = 2;
    int AWAITING_CONNECTION_RESULT = 3;
    int CONNECTED = 4;
  }

  @LayoutRes
  private static int getLayoutForViewType(@ViewType int viewType) {
    switch (viewType) {
      case ViewType.DISCOVERED:
        return R.layout.list_item_device_discovered;
      case ViewType.CONNECTION_REQUESTED:
        return R.layout.list_item_device_connection_requested;
      case ViewType.CONNECTION_INITIATED:
        return R.layout.list_item_device_connection_initiated;
      case ViewType.AWAITING_CONNECTION_RESULT:
        return R.layout.list_item_device_awaiting_connection_result;
      case ViewType.CONNECTED:
        return R.layout.list_item_device_connected;
      default:
        throw new IllegalArgumentException(
            String.format(Locale.US, "Unknown viewType %d", viewType));
    }
  }

  private final FragmentManager fragmentManager;
  private final TextView endpointNameField;
  private final LayoutInflater layoutInflater;

  DeviceAdapter(
      FragmentManager fragmentManager,
      TextView endpointNameField,
      LayoutInflater layoutInflater) {
    this.fragmentManager = fragmentManager;
    this.endpointNameField = endpointNameField;
    this.layoutInflater = layoutInflater;

    setHasStableIds(true);

    // Register for callbacks when devices are added/removed/changed.
    DeviceManager.getInstance().addObserver((o, arg) -> notifyDataSetChanged());
  }

  @Override
  public DeviceViewHolder onCreateViewHolder(ViewGroup parent, @ViewType int viewType) {
    View view =
        LayoutInflater.from(parent.getContext())
            .inflate(getLayoutForViewType(viewType), parent, false);
    switch (viewType) {
      case ViewType.DISCOVERED:
        return new DiscoveredViewHolder(view, endpointNameField, layoutInflater);
      case ViewType.CONNECTION_REQUESTED:
        return new ConnectingViewHolder(view);
      case ViewType.CONNECTION_INITIATED:
      case ViewType.AWAITING_CONNECTION_RESULT:
        return new ConnectionInitiatedViewHolder(view);
      case ViewType.CONNECTED:
        return new ConnectedViewHolder(view, fragmentManager);
      default:
        throw new IllegalArgumentException(
            String.format(Locale.US, "Unknown viewType %d", viewType));
    }
  }

  @ViewType
  @Override
  public int getItemViewType(int position) {
    Device device = getDeviceAt(position);

    switch (device.getDeviceState()) {
      case DISCOVERED:
        return ViewType.DISCOVERED;
      case CONNECTION_REQUESTED:
        return ViewType.CONNECTION_REQUESTED;
      case CONNECTION_INITIATED:
        return ViewType.CONNECTION_INITIATED;
      case AWAITING_CONNECTION_RESULT:
        return ViewType.AWAITING_CONNECTION_RESULT;
      case CONNECTED:
        return ViewType.CONNECTED;
    }

    throw new IllegalArgumentException(
        String.format("Unknown device state %s", device.getDeviceState()));
  }

  @Override
  public void onBindViewHolder(DeviceViewHolder holder, int position) {
    Device device = getDeviceAt(position);
    holder.onBind(device);
  }

  @Override
  public long getItemId(int position) {
    return getDeviceAt(position).getId().hashCode();
  }

  @Override
  public int getItemCount() {
    return DeviceManager.getInstance().getDevices().size();
  }

  private Device getDeviceAt(int position) {
    return DeviceManager.getInstance().getDevices().get(position);
  }

  static class DeviceViewHolder extends ViewHolder {

    // View that represents the overall view of this device.
    private final View deviceView;

    // TextView that shows the remote device found by Nearby Connections.
    private final TextView deviceNameTextView;

    DeviceViewHolder(View view) {
      super(view);

      deviceView = view;
      deviceNameTextView = view.findViewById(R.id.nearbyDeviceName);
    }

    @CallSuper
    void onBind(Device device) {
      // Make sure clicking the device no-ops by default.
      deviceView.setOnClickListener(null);

      deviceNameTextView.setText(device.getName());
    }
  }

  static class DiscoveredViewHolder extends DeviceViewHolder {

    // Button that sends connection request to the corresponding advertising device found by
    // Nearby Connections. This button is placed on the right side of deviceNameTextView after
    // the advertiser was found.
    private final Button connectButton;

    private ImageButton connectionOptionsButton;

    // The adapter which stores user customized mediums.
    private RecyclerViewAdapter mediumsAdapter;
    private AlertDialog mediumsDialog;

    // Text field where we grab the endpoint name from when requesting a connection.
    private final TextView endpointNameField;

    DiscoveredViewHolder(
        View view,
        TextView endpointNameField,
        LayoutInflater layoutInflater) {
      super(view);

      connectButton = view.findViewById(R.id.connectButton);
      connectionOptionsButton = view.findViewById(R.id.connectionOptions);
      this.endpointNameField = endpointNameField;

      View mediumsLayout = layoutInflater.inflate(R.layout.mediums_list, null);
      RecyclerView mediumsRecyclerView = mediumsLayout.findViewById(R.id.recyclerView);
      ArrayList<String> mediums = new ArrayList<>();
      mediums.add("USB");
      mediums.add("WIFI_LAN");
      mediums.add("WIFI_AWARE");
      mediums.add("WIFI_DIRECT");
      mediums.add("WIFI_HOTSPOT");
      mediums.add("WEB_RTC");
      mediums.add("BLUETOOTH");
      mediums.add("BLE");
      mediums.add("BLE_L2CAP");
      mediums.add("NFC");
      mediumsAdapter = new RecyclerViewAdapter(mediums);
      ItemTouchHelper.Callback callback = new ItemMoveCallback(mediumsAdapter);
      ItemTouchHelper touchHelper = new ItemTouchHelper(callback);
      touchHelper.attachToRecyclerView(mediumsRecyclerView);
      mediumsRecyclerView.setAdapter(mediumsAdapter);

      mediumsDialog =
          new AlertDialog.Builder(connectionOptionsButton.getContext())
              .setTitle(R.string.label_connection_options)
              .setView(mediumsLayout)
              .setNeutralButton(R.string.button_done, null)
              .create();
    }

    @Override
    void onBind(Device device) {
      super.onBind(device);

      connectButton.setOnClickListener(
          v -> device.connect(endpointNameField.getText().toString(), mediumsAdapter.getData()));

      connectionOptionsButton.setOnClickListener(v -> mediumsDialog.show());
    }
  }

  static class ConnectingViewHolder extends DeviceViewHolder {

    // Button that sends disconnect request to the corresponding advertising device found by
    // Nearby Connections. This button is placed on the right side of deviceNameTextView after
    // a connection with the advertiser begins.
    private final Button cancelButton;

    ConnectingViewHolder(View view) {
      super(view);

      cancelButton = view.findViewById(R.id.cancelButton);
    }

    @Override
    void onBind(Device device) {
      super.onBind(device);

      cancelButton.setOnClickListener(v -> device.disconnect());
    }
  }

  static class ConnectionInitiatedViewHolder extends DeviceViewHolder {

    // TextView that shows the authentication token for the connecting device.
    private final TextView deviceAuthTokenTextView;

    // Button that accepts connection request from the corresponding discovering device. This button
    // is placed on the right side of deviceNameTextView after the connection request is received
    // from the discoverer (along with rejectButton).
    private final Button acceptButton;

    // Button that rejects connection request from the corresponding discovering device.
    // This button is placed on the right side of deviceNameTextView after the connection request
    // is received from the discoverer (along with acceptButton).
    private final Button rejectButton;

    ConnectionInitiatedViewHolder(View view) {
      super(view);
      deviceAuthTokenTextView = view.findViewById(R.id.nearbyDeviceAuthToken);
      acceptButton = view.findViewById(R.id.acceptButton);
      rejectButton = view.findViewById(R.id.rejectButton);
    }

    @Override
    void onBind(Device device) {
      super.onBind(device);

      acceptButton.setOnClickListener(v -> device.accept());
      rejectButton.setOnClickListener(v -> device.reject());

      deviceAuthTokenTextView.setText(device.getAuthToken());
    }
  }

  static class ConnectedViewHolder extends DeviceViewHolder {
    // View that represents the overall view of this device.
    private final View deviceView;

    // TextView that shows the medium used to connect to the remote device.
    private final TextView connectionMediumTextView;

    // Button that stops the connection to the remote device. This button is placed on the right
    // side of deviceNameTextView after the connection is established between the local device and
    // the remote device (along with transferButton).
    private final Button disconnectButton;

    // FragmentManager used for displaying the payload transfer dialog.
    private final FragmentManager fragmentManager;

    ConnectedViewHolder(View view, FragmentManager fragmentManager) {
      super(view);
      deviceView = view;
      connectionMediumTextView = view.findViewById(R.id.nearbyDeviceMedium);
      disconnectButton = view.findViewById(R.id.disconnectButton);
      this.fragmentManager = fragmentManager;
    }

    @Override
    void onBind(Device device) {
      super.onBind(device);

      deviceView.setOnClickListener(v -> showTransferDialog(device));
      connectionMediumTextView.setText(device.getMedium());
      disconnectButton.setOnClickListener(v -> device.disconnect());
    }

    private void showTransferDialog(Device device) {
      Bundle arguments = new Bundle();
      arguments.putString(PayloadTransferDialogFragment.ARGUMENT_DEVICE_KEY, device.getId());
      PayloadTransferDialogFragment payloadTransferDialogFragment =
          new PayloadTransferDialogFragment();
      payloadTransferDialogFragment.setArguments(arguments);
      payloadTransferDialogFragment.show(fragmentManager, TAG_TRANSFER_DIALOG);
    }
  }
}
