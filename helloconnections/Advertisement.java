package com.google.location.nearby.apps.helloconnections;

import android.util.Log;
import androidx.annotation.Nullable;
import com.google.android.gms.nearby.connection.Device.InstanceType;
import com.google.auto.value.AutoValue;
import java.io.ByteArrayInputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

/**
 * Uses to hold the advertisements used for advertising/discovery.
 *
 * <p>[DEVICE_TOKEN_BIT][PSM_SUPPORT_BIT][INSTANCE_TYPE_BIT][NAME_LENGTH_4_BITS], [ENDPOINT_ID],
 * [DEVICE_TOKEN], [PSM_VALUE], [NAME]
 */
@AutoValue
public abstract class Advertisement {
  private static final int DEVICE_TOKEN_BITMASK = 0b010000000;
  private static final int PSM_BITMASK = 0b001000000;
  private static final int INSTANCE_TYPE_BITMASK = 0b000100000;

  private static final int NAME_LENGTH_BITMASK = 0b000011111;

  // The BT length is just a rough value, and we don't actually test the limit of it. In normal
  // cases, we should never reach the limit of bt name size.
  private static final int MAX_BT_DATA_LENGTH = 50;
  private static final int MAX_BLE_DATA_LENGTH = 27;
  private static final int HEADER_LENGTH = 1;
  private static final int PSM_LENGTH = 2;
  private static final int DEVICE_TOKEN_LENGTH = 2;
  private static final int ENDPOINT_ID_LENGTH = 4;

  public abstract String endpointId();

  public abstract String name();

  @Nullable
  @SuppressWarnings("mutable")
  public abstract byte[] psm();

  @Nullable
  @SuppressWarnings("mutable")
  public abstract byte[] deviceToken();

  public abstract int instanceType();

  public static Builder builder() {
    return new AutoValue_Advertisement.Builder()
        .setPsm(null)
        .setDeviceToken(null)
        .setInstanceType(InstanceType.MAIN);
  }

  /** Builder class for {@link Advertisement}. */
  @AutoValue.Builder
  public abstract static class Builder {
    public abstract Builder setName(String value);

    public abstract Builder setEndpointId(String value);

    public abstract Builder setPsm(@Nullable byte[] psm);

    public abstract Builder setDeviceToken(@Nullable byte[] deviceToken);

    public abstract Builder setInstanceType(int instanceType);

    public abstract Advertisement build();
  }

  @Nullable
  public static Advertisement fromBytes(@Nullable byte[] bytes) {
    if (bytes == null) {
      Log.d(Util.TAG, "Failed to parse null advertisement.");
      return null;
    }

    try {
      ByteArrayInputStream inputStreamStream = new ByteArrayInputStream(bytes);

      // Parse Header.
      byte header = (byte) inputStreamStream.read();
      boolean hasPsm = (header & PSM_BITMASK) == PSM_BITMASK;
      boolean hasDeviceToken = (header & DEVICE_TOKEN_BITMASK) == DEVICE_TOKEN_BITMASK;
      // Binary value 0 as MAIN, 1 as SECONDARY
      int instanceType =
          ((header & INSTANCE_TYPE_BITMASK) == INSTANCE_TYPE_BITMASK)
              ? InstanceType.SECONDARY
              : InstanceType.MAIN;
      int nameLength = header & NAME_LENGTH_BITMASK;


      // Parse Endpoint id.
      byte[] endpointIdBytes = new byte[ENDPOINT_ID_LENGTH];
      if (inputStreamStream.read(endpointIdBytes, 0, ENDPOINT_ID_LENGTH) != ENDPOINT_ID_LENGTH) {
        Log.w(Util.TAG, "Failed to parse endpoint id.");
        return null;
      }
      String endpointId = new String(endpointIdBytes, StandardCharsets.UTF_8);

      // Parse Device Token.
      byte[] deviceToken = null;
      if (hasDeviceToken) {
        deviceToken = new byte[DEVICE_TOKEN_LENGTH];
        if (inputStreamStream.read(deviceToken, 0, DEVICE_TOKEN_LENGTH) != DEVICE_TOKEN_LENGTH) {
          Log.w(Util.TAG, "Failed to parse device token.");
          return null;
        }
      }

      // Parse PSM.
      byte[] psm = null;
      if (hasPsm) {
        psm = new byte[PSM_LENGTH];
        if (inputStreamStream.read(psm, 0, PSM_LENGTH) != PSM_LENGTH) {
          Log.w(Util.TAG, "Failed to parse psm.");
          return null;
        }
      }

      // Parse Name.
      byte[] nameBytes = new byte[nameLength];
      if (inputStreamStream.read(nameBytes, 0, nameLength) != nameLength) {
        Log.w(Util.TAG, "Failed to parse name.");
        return null;
      }
      String name = new String(nameBytes, StandardCharsets.UTF_8);

      return builder()
          .setEndpointId(endpointId)
          .setPsm(psm)
          .setName(name)
          .setDeviceToken(deviceToken)
          .setInstanceType(instanceType)
          .build();
    } catch (Exception e) {
      return null;
    }
  }

  public byte[] toBleBytes() {
    return toBytes(MAX_BLE_DATA_LENGTH);
  }

  public byte[] toBtBytes() {
    return toBytes(MAX_BT_DATA_LENGTH);
  }

  private byte[] toBytes(int maxLength) {
    boolean hasPsm = psm() != null;
    boolean hasDeviceToken = deviceToken() != null;
    int reservedDataLength = ENDPOINT_ID_LENGTH + HEADER_LENGTH;
    if (hasPsm) {
      reservedDataLength += PSM_LENGTH;
    }
    if (hasDeviceToken) {
      reservedDataLength += DEVICE_TOKEN_LENGTH;
    }
    String displayName = getValidNameForLegacyBle(name(), maxLength - reservedDataLength);
    byte[] nameBytes = displayName.getBytes(StandardCharsets.UTF_8);

    byte header = hasPsm ? (byte) PSM_BITMASK : 0;
    header |= hasDeviceToken ? DEVICE_TOKEN_BITMASK : 0;
    // Binary value 0 as MAIN, 1 as SECONDARY
    header |= (instanceType() == InstanceType.MAIN) ? 0 : INSTANCE_TYPE_BITMASK;
    header |= (nameBytes.length & NAME_LENGTH_BITMASK);

    ByteBuffer byteBuffer = ByteBuffer.allocate(reservedDataLength + nameBytes.length);
    // Header.
    byteBuffer.put(header);

    // Endpoint id.
    byte[] endpointIdBytes = endpointId().getBytes(StandardCharsets.UTF_8);
    byteBuffer.put(endpointIdBytes);

    // DeviceToken.
    if (hasDeviceToken) {
      byteBuffer.put(deviceToken());
    }

    // PSM
    if (hasPsm) {
      byteBuffer.put(psm());
    }

    // Name.
    byteBuffer.put(nameBytes);
    return byteBuffer.array();
  }

  private String getValidNameForLegacyBle(String originName, int maxLength) {
    if (originName.getBytes(StandardCharsets.UTF_8).length <= maxLength) {
      return originName;
    }

    // Strip the name string by 1 and see if it's valid now.
    return getValidNameForLegacyBle(originName.substring(0, originName.length() - 1), maxLength);
  }
}
