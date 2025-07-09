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

// A BleInputStream represents an input stream of bytes from a Ble socket.
class BleInputStream : public InputStream {
 public:
  BleInputStream(InputStream& source) : source_(source) {}

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  InputStream& source_;
};

// A BleOutputStream represents an output stream of bytes to a Ble socket.
class BleOutputStream : public OutputStream {
 public:
  BleOutputStream(OutputStream& source, const ByteArray& service_id_hash)
      : source_(source), service_id_hash_(service_id_hash) {}

  Exception Write(const ByteArray& data) override;

  Exception Flush() override;
  Exception Close() override;

 private:
  OutputStream& source_;
  const ByteArray service_id_hash_;
};

// A BleSocket represents a socket over BLE or BLE_L2CAP.
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

  // TODO: edwinwu - Implement these methods once the L2CAP refactor begins.
  ExceptionOr<ByteArray> DispatchPacket() ABSL_LOCKS_EXCLUDED(mutex_) {
    return ExceptionOr<ByteArray>(Exception::kFailed);
  }
  Exception WritePayloadLength(int payload_length) ABSL_LOCKS_EXCLUDED(mutex_) {
    return {Exception::kFailed};
  }
  Exception SendIntroduction() ABSL_LOCKS_EXCLUDED(mutex_){
    return {Exception::kFailed};
  }
  Exception SendDisconnection() ABSL_LOCKS_EXCLUDED(mutex_){
    return {Exception::kFailed};
  }
  Exception SendPacketAcknowledgement(int received_size)
      ABSL_LOCKS_EXCLUDED(mutex_){
        return {Exception::kFailed};
      }
  Exception ProcessIncomingL2capPacketValidation() ABSL_LOCKS_EXCLUDED(mutex_){
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
