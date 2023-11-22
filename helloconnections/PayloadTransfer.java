package com.google.location.nearby.apps.helloconnections;

import static com.google.location.nearby.apps.helloconnections.Util.TAG;
import static com.google.location.nearby.apps.helloconnections.Util.copyFileFromUri;

import android.content.Context;
import android.os.SystemClock;
import android.util.Log;
import com.google.android.gms.nearby.Nearby;
import com.google.android.gms.nearby.connection.Payload;
import com.google.android.gms.nearby.connection.PayloadTransferUpdate;
import java.io.IOException;
import java.io.InputStream;

/** Represent a payload transfer between Nearby devices. */
public class PayloadTransfer {

  private final Payload payload;
  private final String name;
  private final boolean isIncoming;
  private final Context context;

  @PayloadTransferUpdate.Status private int status;
  private long bytesTransferred;

  static PayloadTransfer createIncomingPayloadTransfer(Context context, Payload payload) {
    return new PayloadTransfer(context, payload, /* isIncoming= */ true);
  }

  static PayloadTransfer createOutgoingPayloadTransfer(Context context, Payload payload) {
    return new PayloadTransfer(context, payload, /* isIncoming= */ false);
  }

  private PayloadTransfer(Context context, Payload payload, boolean isIncoming) {
    this.context = context;
    this.payload = payload;
    this.isIncoming = isIncoming;

    switch (payload.getType()) {
      case Payload.Type.BYTES:
        name = "BYTES";
        break;
      case Payload.Type.FILE:
        name = "FILE";
        break;
      case Payload.Type.STREAM:
        name = "STREAM";
        break;
      default:
        name = "UNKNOWN";
    }
  }

  void cancel(Context context) {
    Nearby.getConnectionsClient(context).cancelPayload(payload.getId());
  }

  long getPayloadId() {
    return payload.getId();
  }

  String getName() {
    return name;
  }

  @PayloadTransferUpdate.Status
  int getStatus() {
    return status;
  }

  boolean isComplete() {
    return status == PayloadTransferUpdate.Status.SUCCESS;
  }

  long getBytesTransferred() {
    return bytesTransferred;
  }

  /**
   * Returns the total number of bytes expected in this payload.
   *
   * @return the total number of bytes, or -1 if unknown
   */
  long getTotalBytes() {
    switch (payload.getType()) {
      case Payload.Type.BYTES:
        return payload.asBytes().length;
      case Payload.Type.FILE:
        return payload.asFile().getSize();
      case Payload.Type.STREAM:
        // Fall through.
      default:
        return -1;
    }
  }

  /**
   * Returns the percentage of this payload transferred.
   *
   * @return the payload transfer percentage between 0 and 100, or -1 if the percentage is unknown
   */
  int getTransferredPercentage() {
    if (getTotalBytes() == -1) {
      return -1;
    }

    return (int) (100 * ((float) bytesTransferred / getTotalBytes()));
  }

  private long payloadId;
  private Thread readStreamInBgThread;
  private static final long READ_STREAM_IN_BG_TIMEOUT = 5000;

  private void readStreamInBg(InputStream inputStream) {
    // Override the thread class
    readStreamInBgThread =
        new Thread() {
          @Override
          public void run() {
            long lastRead = SystemClock.elapsedRealtime();
            while (!Thread.interrupted()) {
              if ((SystemClock.elapsedRealtime() - lastRead) >= READ_STREAM_IN_BG_TIMEOUT) {
                Log.e(TAG, "Read data from stream but timed out.");
                break;
              }

              try {
                int availableBytes = inputStream.available();
                if (availableBytes > 0) {
                  byte[] bytes = new byte[availableBytes];
                  // read bytes but do nothing
                  inputStream.read(bytes);
                  lastRead = SystemClock.elapsedRealtime();
                }
              } catch (IOException e) {
                Log.e(TAG, "Failed to read bytes from InputStream.", e);
                break;
              } // try-catch
            } // while
          }
        };
    readStreamInBgThread.start();
  }

  void notifyUpdate(PayloadTransferUpdate update) {
    // If this is an incoming STREAM payload, make sure to read all available bytes from the input
    // stream.
    if (payload.getType() == Payload.Type.STREAM && isIncoming) {
      if (update.getPayloadId() != payloadId) {
        payloadId = update.getPayloadId();
        // Read bytes in BT with while loop to make improve the throughput from 3.xx MB/s to 15 MB/s
        // up.
        readStreamInBg(payload.asStream().asInputStream());
      } else if (update.getStatus() != PayloadTransferUpdate.Status.IN_PROGRESS) {
        readStreamInBgThread.interrupt();
      }
    }

    bytesTransferred = update.getBytesTransferred();
    status = update.getStatus();

    if (isIncoming && isComplete() && payload.getType() == Payload.Type.FILE) {
      copyFileFromUri(context, payload.asFile().asUri());
    }
  }
}
