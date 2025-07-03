// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_BLE_SOCKET_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_BLE_SOCKET_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace connections {
namespace mediums {

/**
 * A decorator for an `InputStream` that filters protocol-specific data
 * for BLE connections.
 *
 * This class wraps a raw `InputStream` from an underlying BLE socket (either
 * GATT or L2CAP). Its primary function is to intercept the incoming byte stream
 * and filter out the `service_id_hash` before passing the data to the upper
 * layers of the Nearby Connections protocol. This ensures that clients of
 * `BleSocket` receive only the application payload.
 *
 * This stream does not own the underlying `source_` stream; it holds a
 * reference and depends on the owner of the `source_` to manage its
 * lifetime. It is intended for internal use by the `BleSocket` class.
 */
class BleInputStream : public InputStream {
 public:
  BleInputStream(InputStream& source) : source_(source) {}

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  InputStream& source_;
};

/**
 * A decorator for an `OutputStream` that prepends protocol-specific
 * data for BLE connections.
 *
 * This class wraps a raw `OutputStream` from an underlying BLE socket (GATT or
 * L2CAP). Its main responsibility is to prepend the `service_id_hash` to every
 * outgoing data packet before it is written to the physical socket. This is a
 * requirement of the Nearby Connections protocol to ensure that the remote
 * device can identify the service and demultiplex the connection.
 *
 * This stream does not own the underlying `source_` stream; it holds a
 * reference and relies on the owner of the `source_` to manage its lifetime.
 * It is used internally by the `BleSocket` class to handle the low-level
 * details of packet formatting.
 */
class BleOutputStream : public OutputStream {
 public:
  BleOutputStream(OutputStream& source, const ByteArray& service_id_hash)
      : source_(source), service_id_hash_(service_id_hash) {}

  Exception Write(const ByteArray& data) override;

  Exception Flush() override;
  Exception Close() override;

  /**
   * Sends the length of a data payload to the remote endpoint.
   *
   * This method prepares the socket for an upcoming data payload by sending its
   * length, including the necessary `service_id_hash` prefix.
   *
   * This method must be called immediately before sending the corresponding
   * payload.
   *
   * @param payload_length The length, in bytes, of the upcoming payload.
   * @return An `Exception` object indicating the status of the write
   * operation. `{Exception::kSuccess}` on success.
   */
  Exception WritePayloadLength(int payload_length);

 private:
  OutputStream& source_;
  const ByteArray service_id_hash_;
  int payload_length_ = 0;
};

/**
 * A decorator for BLE (GATT) and BLE L2CAP sockets that centralizes
 * connection and packet-handling logic for Nearby Connections.
 *
 * This class wraps an underlying `BleV2Socket` (for GATT) or `BleL2capSocket`
 * to provide a unified interface for data transfer. Its primary responsibility
 * is to centralize BLE and L2CAP packet validation within the Nearby
 * Connections protocol layer. This includes handling `service_id_hash`
 * validation and managing packet prefixing and formatting.
 *
 * It intercepts the raw input and output streams of the underlying socket using
 * custom `BleInputStream` and `BleOutputStream` wrappers to inject
 * protocol-specific logic. This includes sending introduction frames, handling
 * disconnections, and validating L2CAP packets before they are sent or
 * received.
 *
 * All operations on this socket are serialized through an internal
 * `SingleThreadExecutor` to ensure thread safety and correct ordering of
 * asynchronous I/O operations.
 *
 * Instances of this class must be created using one of the static factory
 * methods: `CreateWithBleSocket()` or `CreateWithL2capSocket()`.
 */
class BleSocket final {
 public:
  // Factory methods for creating a BleSocket instance.
  static std::unique_ptr<BleSocket> CreateWithBleSocket(
      BleV2Socket ble_socket, const ByteArray& service_id_hash) {
    return std::unique_ptr<BleSocket>(new BleSocket(
        service_id_hash,
        std::make_unique<BleInputStream>(ble_socket.GetInputStream()),
        std::make_unique<BleOutputStream>(ble_socket.GetOutputStream(),
                                          service_id_hash),
        std::move(ble_socket)));
  }

  static std::unique_ptr<BleSocket> CreateWithL2capSocket(
      BleL2capSocket l2cap_socket, const ByteArray& service_id_hash) {
    return std::unique_ptr<BleSocket>(new BleSocket(
        service_id_hash,
        std::make_unique<BleInputStream>(l2cap_socket.GetInputStream()),
        std::make_unique<BleOutputStream>(l2cap_socket.GetOutputStream(),
                                          service_id_hash),
        std::move(l2cap_socket)));
  }

  ~BleSocket();

  InputStream& GetInputStream() ABSL_LOCKS_EXCLUDED(mutex_);
  OutputStream& GetOutputStream() ABSL_LOCKS_EXCLUDED(mutex_);
  Exception Close() ABSL_LOCKS_EXCLUDED(mutex_);
  BleV2Peripheral& GetRemotePeripheral() ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsValid() const ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * Returns the medium used by this socket.
   *
   * @return The medium used by this socket.
   */
  ::location::nearby::proto::connections::Medium GetMedium() const
      ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * Dispatches the next logical packet from the socket by routing it
   * based on its type.
   *
   * This is the primary entry point for reading from the socket and is
   * responsible for parsing the underlying byte stream to identify the next
   * packet.
   *
   * The function handles protocol-level demultiplexing by inspecting the
   * packet's service ID hash to differentiate control packets from data
   * packets.
   *
   * For data packets, this function reads and returns only the
   * application-level payload, not the full BLE packet. For control packets, it
   * fully consumes and processes the packet by delegating to an internal
   * handler, and the returned `ByteArray` may be empty as some control packets
   * carry no payload.
   *
   * @return An `ExceptionOr` containing the `ByteArray` payload of a data
   *     packet on success. The `ByteArray` may be empty for certain control
   *     packets that have no payload. Returns an `Exception` if a protocol
   *     error occurs or the read operation fails.
   */
  ExceptionOr<ByteArray> DispatchPacket() ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * Reads the length of the next data payload from the socket.
   *
   * This method is used to read the length of the next data payload from the
   * socket. It is intended to be called immediately after `DispatchPacket()`
   * to retrieve the length of the payload that was just read.
   *
   * @return An `ExceptionOr` containing the length of the next data payload on
   *     success. Returns an `Exception` if a protocol error occurs or the read
   *     operation fails.
   */
  ExceptionOr<std::int32_t> ReadPayloadLength() ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * Sends the length of a data payload to the remote endpoint.
   *
   * This method prepares the socket for an upcoming data payload by sending its
   * length. It handles the necessary protocol formatting, including prepending
   * the `service_id_hash` to the length information.
   *
   * This method should be called immediately before sending the corresponding
   * payload.
   *
   * @param payload_length The length, in bytes, of the upcoming payload.
   * @return An `Exception` object indicating the status of the write
   * operation. `{Exception::kSuccess}` on success.
   */
  Exception WritePayloadLength(int payload_length) ABSL_LOCKS_EXCLUDED(mutex_);

  Exception SendIntroduction() ABSL_LOCKS_EXCLUDED(mutex_) {
    return {Exception::kFailed};
  }
  Exception SendDisconnection() ABSL_LOCKS_EXCLUDED(mutex_) {
    return {Exception::kFailed};
  }
  Exception SendPacketAcknowledgement(int received_size)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    return {Exception::kFailed};
  }
  Exception ProcessIncomingL2capPacketValidation() ABSL_LOCKS_EXCLUDED(mutex_) {
    return {Exception::kFailed};
  }
  Exception ProcessOutgoingL2capPacketValidation() ABSL_LOCKS_EXCLUDED(mutex_) {
    return {Exception::kFailed};
  }

 private:
  BleSocket(const ByteArray& service_id_hash,
            std::unique_ptr<BleInputStream> ble_input_stream,
            std::unique_ptr<BleOutputStream> ble_output_stream,
            BleV2Socket ble_socket);

  BleSocket(const ByteArray& service_id_hash,
            std::unique_ptr<BleInputStream> ble_input_stream,
            std::unique_ptr<BleOutputStream> ble_output_stream,
            BleL2capSocket l2cap_socket);

  ::location::nearby::proto::connections::Medium GetMediumLocked() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Exception CloseLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  const ByteArray service_id_hash_;
  std::unique_ptr<mediums::BleInputStream> ble_input_stream_
      ABSL_GUARDED_BY(mutex_) = nullptr;
  std::unique_ptr<mediums::BleOutputStream> ble_output_stream_
      ABSL_GUARDED_BY(mutex_) = nullptr;
  BleV2Socket ble_socket_ ABSL_GUARDED_BY(mutex_) = BleV2Socket();
  BleL2capSocket l2cap_socket_ ABSL_GUARDED_BY(mutex_) = BleL2capSocket();
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_BLE_SOCKET_H_
