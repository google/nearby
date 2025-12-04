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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_BLE_SOCKET_H_
#define CORE_INTERNAL_MEDIUMS_BLE_BLE_SOCKET_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "internal/platform/ble.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/mutex.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/runnable.h"
#include "internal/platform/single_thread_executor.h"

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
  explicit BleInputStream(InputStream& source) : source_(source) {}

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

  /**
   * Writes data to the stream by first creating a self-delimited packet.
   *
   * This method packetizes the given `data` before writing it to the underlying
   * `source_` output stream. The packetization process is a requirement of the
   * Nearby Connections protocol to ensure the remote device can correctly
   * parse streamed data.
   *
   * Internally, it uses `BlePacket::CreateDataPacket` to construct a data
   * packet. This involves prepending the data's length and the
   * `service_id_hash_` to the actual payload.
   *
   * Prepending the length allows the receiver to determine message boundaries
   * when reading from the stream, which is a common practice for streaming
   * protocols. The `service_id_hash_` allows the remote device to identify the
   * service and demultiplex the connection.
   *
   * The resulting serialized `BlePacket` is then written to the `source_`
   * stream.
   *
   * @param data The raw `ByteArray` payload to write to the stream.
   * @return `Exception::kSuccess` if the write operation succeeds, or an
   *         exception code indicating the type of error.
   */
  Exception Write(const ByteArray& data) override;

  Exception Flush() override;
  Exception Close() override;

  /**
   * Sends a protocol-level control packet over the BLE socket.
   *
   * Control packets are used for managing the connection state, such as sending
   * introduction frames, acknowledgements, or disconnection messages, as
   * distinct from bulk payload data transfer.
   *
   * This method is a decorated API on the `BleOutputStream`. It ensures the
   * packet is formatted according to the Nearby Connections protocol before
   * being written to the underlying stream. This includes automatically
   * prepending the 3-byte `service_id_hash` to the packet data.
   *
   * @param data The `ByteArray` containing the raw content of the control
   * packet to be sent.
   * @return An `Exception` object indicating the status of the write
   * operation. `{Exception::kSuccess}` on success.
   */
  Exception WriteControlPacket(const ByteArray& data);

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
 * This class wraps an underlying `BleSocket` (for GATT) or `BleL2capSocket`
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
      nearby::BleSocket ble_socket, const ByteArray& service_id_hash) {
    return std::unique_ptr<BleSocket>(new BleSocket(
        service_id_hash,
        std::make_unique<BleInputStream>(ble_socket.GetInputStream()),
        std::make_unique<BleOutputStream>(ble_socket.GetOutputStream(),
                                          service_id_hash),
        std::move(ble_socket)));
  }

  static std::unique_ptr<BleSocket> CreateWithL2capSocket(
      nearby::BleL2capSocket l2cap_socket, const ByteArray& service_id_hash) {
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
  nearby::BlePeripheral& GetRemotePeripheral() ABSL_LOCKS_EXCLUDED(mutex_);
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

  /**
   *  Sends the initial introduction packet to the remote endpoint.
   *
   * This function is called immediately after a new BLE connection is
   * established. Its purpose is to initiate the Nearby Connections protocol
   * handshake by sending a control packet containing the `service_id_hash`.
   * This allows the remote device to validate that the connection is intended
   * for the correct service.
   *
   * This operation is part of the pre-connection setup and is typically the
   * first packet sent over a new socket.
   *
   * @return An `Exception` object indicating the status of the write
   * operation. `{Exception::kSuccess}` on success.
   */
  Exception SendIntroduction() ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * Sends a disconnection packet to the remote endpoint.
   *
   * This function is used to gracefully terminate the connection at the
   * protocol level. It sends an explicit control packet to inform the remote
   * device that the connection is being closed intentionally.
   *
   * This is typically called as part of the teardown process for a connection,
   * ensuring the remote endpoint is aware of the state change.
   *
   * @return An `Exception` object indicating the status of the write
   * operation. `{Exception::kSuccess}` on success.
   */
  Exception SendDisconnection() ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * Sends a packet acknowledgement to the remote endpoint.
   *
   * This function is used to confirm the receipt of a data packet at the
   * protocol level. After successfully receiving a data packet, this method
   * should be called to send a control packet back to the sender, which
   * includes the size of the packet that was received.
   *
   * This acknowledgement mechanism allows the sender to verify that its data
   * was successfully delivered.
   *
   * @param received_size The size, in bytes, of the data packet that was just
   * received and is being acknowledged.
   * @return An `Exception` object indicating the status of the write
   * operation. `{Exception::kSuccess}` on success.
   */
  Exception SendPacketAcknowledgement(int received_size)
      ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * Processes the server-side L2CAP connection validation handshake.
   *
   * Its primary role is to manage the handshake required to
   * establish a dedicated data channel after an initial L2CAP connection has
   * been made.
   *
   * The function waits to receive a `Command::kRequestDataConnection` packet
   * from the remote device. Upon receiving this request, it proceeds to
   * validate the connection and, if successful, responds by sending a
   * `Command::kResponseDataConnectionReady` packet to the initiator to signal
   * that the data channel is established and ready for use.
   *
   * @return An `Exception` object indicating the status of the validation
   * process. `{Exception::kSuccess}` is returned if the handshake completes
   * successfully.
   */
  Exception ProcessIncomingL2capPacketValidation() ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * Processes the client-side L2CAP connection validation handshake.
   *
   * Its primary role is to initiate the handshake required to establish a
   * dedicated data channel after an initial L2CAP connection has been made.
   *
   * The function begins by sending a `Command::kRequestDataConnection` packet
   * to the remote device to request the creation of a data channel. It then
   * waits to receive a `Command::kResponseDataConnectionReady` packet from the
   * acceptor, which signals that the data channel has been successfully
   * established and is ready for use.
   *
   * @return An `Exception` object indicating the status of the validation
   * process. `{Exception::kSuccess}` is returned if the handshake completes
   * successfully.
   */
  Exception ProcessOutgoingL2capPacketValidation() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  BleSocket(const ByteArray& service_id_hash,
            std::unique_ptr<BleInputStream> ble_input_stream,
            std::unique_ptr<BleOutputStream> ble_output_stream,
            nearby::BleSocket ble_socket);

  BleSocket(const ByteArray& service_id_hash,
            std::unique_ptr<BleInputStream> ble_input_stream,
            std::unique_ptr<BleOutputStream> ble_output_stream,
            nearby::BleL2capSocket l2cap_socket);

  /**
   * Processes a control packet received from the socket.
   *
   * This function is called when a control packet is received from the socket.
   * It is responsible for parsing the packet to identify its type and
   * determining the appropriate action.
   *
   * @return An `ExceptionOr<ByteArray>` containing the payload of the
   * received packet on success. For certain control packets that have no
   * payload, the `ByteArray` may be empty. Returns an `Exception` if a
   * protocol error occurs or the read operation fails.
   */
  ExceptionOr<ByteArray> ProcessBleControlPacketLocked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  /**
   * Sends a raw L2CAP packet over the socket.
   *
   * This function provides a direct, low-level interface for transmitting a
   * pre-formatted L2CAP packet. It is intended for use when the caller has
   * already constructed a complete L2CAP packet (e.g., for a specific command
   * or data payload) and needs to send it directly.
   *
   * This method bypasses the higher-level handshake logic encapsulated in
   * `ProcessIncomingL2capPacketValidation` and
   * `ProcessOutgoingL2capPacketValidation`.
   *
   * @param packet_byte A `ByteArray` containing the raw, complete L2CAP
   * packet to be sent over the socket.
   * @return An `Exception` object indicating the status of the write
   * operation. `{Exception::kSuccess}` on success.
   */
  Exception SendL2capPacketLocked(const ByteArray& packet_byte)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void RunOnSocketThread(Runnable runnable);
  ::location::nearby::proto::connections::Medium GetMediumLocked() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Exception CloseLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  SingleThreadExecutor serial_executor_;

  const ByteArray service_id_hash_;
  std::unique_ptr<mediums::BleInputStream> ble_input_stream_
      ABSL_GUARDED_BY(mutex_) = nullptr;
  std::unique_ptr<mediums::BleOutputStream> ble_output_stream_
      ABSL_GUARDED_BY(mutex_) = nullptr;
  nearby::BleSocket ble_socket_ ABSL_GUARDED_BY(mutex_) = nearby::BleSocket();
  nearby::BleL2capSocket l2cap_socket_ ABSL_GUARDED_BY(mutex_) =
      nearby::BleL2capSocket();
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_BLE_SOCKET_H_
