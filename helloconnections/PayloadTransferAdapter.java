package com.google.location.nearby.apps.helloconnections;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.TextView;
import com.google.android.gms.nearby.connection.PayloadTransferUpdate;
import com.google.location.nearby.apps.helloconnections.PayloadTransferAdapter.PayloadTransferViewHolder;

/** An adapter to manage a list of remote {@link Device} objects. */
public class PayloadTransferAdapter extends RecyclerView.Adapter<PayloadTransferViewHolder> {

  private final Context context;
  private final Device device;

  PayloadTransferAdapter(Context context, Device device) {
    this.context = context;
    this.device = device;

    setHasStableIds(true);
  }

  @Override
  public PayloadTransferViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
    return new PayloadTransferViewHolder(
        LayoutInflater.from(parent.getContext())
            .inflate(R.layout.list_item_payload_transfer, parent, false),
        context);
  }

  @Override
  public void onBindViewHolder(PayloadTransferViewHolder holder, int position) {
    holder.onBind(getPayloadTransferAt(position));
  }

  @Override
  public long getItemId(int position) {
    return getPayloadTransferAt(position).getPayloadId();
  }

  @Override
  public int getItemCount() {
    return device.getPayloadTransferList().size();
  }

  private PayloadTransfer getPayloadTransferAt(int position) {
    return device.getPayloadTransferList().get(position);
  }

  static class PayloadTransferViewHolder extends ViewHolder {

    private final Context context;

    private final View payloadTransferView;
    private final TextView nameTextView;
    private final TextView statusTextView;
    private final ProgressBar progressBar;
    private final TextView progressDetailTextView;
    private final TextView payloadIdTextView;

    PayloadTransferViewHolder(View view, Context context) {
      super(view);
      this.context = context;

      payloadTransferView = view;
      nameTextView = view.findViewById(R.id.name);
      statusTextView = view.findViewById(R.id.status);
      progressBar = view.findViewById(R.id.progress_bar);
      progressDetailTextView = view.findViewById(R.id.progress_detail);
      payloadIdTextView = view.findViewById(R.id.payload_id);
    }

    void onBind(PayloadTransfer payloadTransfer) {
      payloadTransferView.setOnLongClickListener(
          v -> {
            payloadTransfer.cancel(context);
            return true;
          });

      nameTextView.setText(payloadTransfer.getName());
      progressBar.setProgress(payloadTransfer.getTransferredPercentage());
      payloadIdTextView.setText(
          context
              .getResources()
              .getString(
                  R.string.payload_transfer_description_payload_id,
                  payloadTransfer.getPayloadId()));

      // Display the status next to the name.
      switch (payloadTransfer.getStatus()) {
        case PayloadTransferUpdate.Status.IN_PROGRESS:
          statusTextView.setText(R.string.payload_transfer_description_status_in_progress);
          break;
        case PayloadTransferUpdate.Status.SUCCESS:
          statusTextView.setText(R.string.payload_transfer_description_status_success);
          break;
        case PayloadTransferUpdate.Status.FAILURE:
          statusTextView.setText(R.string.payload_transfer_description_status_failure);
          break;
        case PayloadTransferUpdate.Status.CANCELED:
          statusTextView.setText(R.string.payload_transfer_description_status_canceled);
          break;
        default:
          statusTextView.setText(R.string.payload_transfer_description_status_unknown);
      }

      // Display the progress based on whether or not we know the total size.
      if (payloadTransfer.getTotalBytes() == -1) {
        progressBar.setIndeterminate(true);
        progressDetailTextView.setText(
            context
                .getResources()
                .getString(
                    R.string.payload_transfer_description_bytes_transferred_size_unknown,
                    payloadTransfer.getBytesTransferred()));
      } else {
        progressBar.setIndeterminate(false);
        progressDetailTextView.setText(
            context
                .getResources()
                .getString(
                    R.string.payload_transfer_description_bytes_transferred_size_known,
                    payloadTransfer.getBytesTransferred(),
                    payloadTransfer.getTotalBytes()));
      }

      // Mark the progress bar at 100% if the payload is complete.
      if (payloadTransfer.isComplete()) {
        progressBar.setIndeterminate(false);
        progressBar.setProgress(100);
      }
    }
  }
}
