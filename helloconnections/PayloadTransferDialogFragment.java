package com.google.location.nearby.apps.helloconnections;

import static com.google.location.nearby.apps.helloconnections.Util.TAG;

import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import com.google.android.gms.nearby.connection.Payload;
import com.google.location.nearby.apps.helloconnections.DeviceManager.DeviceEvent;
import java.util.Observable;
import java.util.Observer;

/**
 * A DialogFragment to show the Payloads being transferred between the local device and another
 * endpoint.
 */
public class PayloadTransferDialogFragment extends DialogFragment implements Observer {

  static final String ARGUMENT_DEVICE_KEY = "device";

  private PayloadTransferAdapter payloadTransferAdapter;

  private Device device;

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    DeviceManager.getInstance().addObserver(this);

    View rootView = inflater.inflate(R.layout.payload_transfer_dialog_fragment, container, false);
    EditText sizeField = rootView.findViewById(R.id.size);
    Button sendBytesButton = rootView.findViewById(R.id.send_bytes);
    Button sendFileButton = rootView.findViewById(R.id.send_file);
    Button sendStreamButton = rootView.findViewById(R.id.send_stream);
    RecyclerView payloadTransfersRecyclerView = rootView.findViewById(R.id.payload_transfers);

    sendBytesButton.setOnClickListener(
        v -> device.sendPayload(extractPayloadSize(sizeField), Payload.Type.BYTES));
    sendFileButton.setOnClickListener(
        v -> device.sendPayload(extractPayloadSize(sizeField), Payload.Type.FILE));
    sendStreamButton.setOnClickListener(
        v -> device.sendPayload(extractPayloadSize(sizeField), Payload.Type.STREAM));

    String id = getArguments().getString(ARGUMENT_DEVICE_KEY);
    device = DeviceManager.getInstance().getDevice(id);
    if (device == null) {
      Log.w(
          TAG,
          String.format(
              "PayloadTransferDialogFragment received an unknown device with ID %s.", id));
      dismiss();
      return rootView;
    }

    payloadTransferAdapter = new PayloadTransferAdapter(getContext(), device);
    payloadTransfersRecyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
    payloadTransfersRecyclerView.setAdapter(payloadTransferAdapter);

    return rootView;
  }

  @Override
  public void onDestroyView() {
    // The observable 'Device' should only take effect when the PayloadTransferDialogFragment is
    // in the foreground.
    DeviceManager.getInstance().deleteObserver(this);
    super.onDestroyView();
  }

  @Override
  public void update(Observable observable, Object event) {
    if (DeviceEvent.PAYLOAD_CHANGE == event) {
      payloadTransferAdapter.notifyDataSetChanged();
    }

    // Check if this device is still active.
    if (DeviceManager.getInstance().getDevice(device.getId()) == null) {
      Util.logErrorAndToast(
          getActivity(), R.string.error_toast_remote_device_disconnected, device.getName());
      dismiss();
    }
  }

  private int extractPayloadSize(EditText sizeField) {
    // Convert the inputted size (KB) into bytes.
    return Integer.parseInt(sizeField.getText().toString()) * 1024;
  }
}
