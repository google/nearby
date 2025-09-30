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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_BLE_L2CAP_PACKET_H_
#define CORE_INTERNAL_MEDIUMS_BLE_BLE_L2CAP_PACKET_H_

#include <string>

#include "absl/status/statusor.h"
#include "connections/implementation/mediums/ble/ble_advertisement.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace mediums {

// Represents the format of the BLE L2CAP packet used in L2CAP socket for
// requesting/responsing.
//
// [COMMAND] or [COMMAND][LENGTH][DATA]
//
class BleL2capPacket {
 public:
  enum class Command {
    // Use to fetch advertisement from server with serviceId.
    kRequestAdvertisement = 1,
    // Use to notify the server have fetched all advertisements completely.
    kRequestAdvertisementFinish = 2,
    // Use to notify the server current L2CAP socket use for file transferring.
    kRequestDataConnection = 3,
    // 11-20 reserved
    // Use to response the advertisement raw data to the client.
    kResponseAdvertisement = 21,
    // Use to response the queried service ID not exist in server side.
    kResponseServiceIdNotFound = 22,
    // Use to notify the data connection ready
    kResponseDataConnectionReady = 23,
    // Use to notify failure for requesting the data connection.
    kResponseDataConnectionFailure = 24,
  };

  static constexpr int kCommandLength = 1;
  static constexpr int kDataLength = 2;
  // The maximum length of a BleL2capPacket is the length when carry the
  // advertisement raw data.
  static constexpr int kMaxLength =
      kCommandLength + kDataLength + BleAdvertisement::kMaxAdvertisementLength;

  // Creates a BleL2capPacket from the raw bytes.
  static absl::StatusOr<BleL2capPacket> CreateFromBytes(const ByteArray& bytes);
  // Creates a BleL2capPacket for requesting advertisement with service ID.
  static absl::StatusOr<ByteArray> ByteArrayForRequestAdvertisement(
      const std::string& service_id);
  // Creates a BleL2capPacket for responding advertisement with raw data.
  static absl::StatusOr<ByteArray> ByteArrayForResponseAdvertisement(
      const ByteArray& advertisement);
  // Creates a BleL2capPacket for requesting advertisement finish.
  static ByteArray ByteArrayForRequestAdvertisementFinish();
  // Creates a BleL2capPacket for responding service ID not found.
  static ByteArray ByteArrayForServiceIdNotFound();
  // Creates a BleL2capPacket for requesting data connection.
  static ByteArray ByteArrayForRequestDataConnection();
  // Creates a BleL2capPacket for responding data connection ready.
  static ByteArray ByteArrayForDataConnectionReady();
  // Creates a BleL2capPacket for responding data connection failure.
  static ByteArray ByteArrayForDataConnectionFailure();
  // Generates the service ID hash bytes from the service ID string.
  static ByteArray GenerateServiceIdHash(const std::string& service_id);
  // Checks if the length of the BleL2capPacket is valid.
  static bool IsValidLength(int length) {
    return length > 0 && length <= kMaxLength;
  }

  BleL2capPacket(Command command, const ByteArray* service_id_hash,
                 const ByteArray* data);
  BleL2capPacket(const BleL2capPacket&) = default;
  BleL2capPacket& operator=(const BleL2capPacket&) = default;
  BleL2capPacket(BleL2capPacket&&) = default;
  BleL2capPacket& operator=(BleL2capPacket&&) = default;
  ~BleL2capPacket() = default;

  explicit operator ByteArray() const;

  // The service ID hash bytes, only exist for command REQUEST_ADVERTISEMENT and
  // Command.REQUEST_DATA_CONNECTION.
  ByteArray GetServiceIdHash() const { return service_id_hash_; }
  // The advertisement raw bytes, only exist for command RESPONSE_ADVERTISEMENT.
  ByteArray GetAdvertisement() const { return advertisement_; }
  bool IsErrorServiceIdNotFound() const {
    return command_ == Command::kResponseServiceIdNotFound;
  }
  bool IsAdvertisementResponse() const {
    return command_ == Command::kResponseAdvertisement;
  }
  bool IsFetchAdvertisementRequest() const {
    return command_ == Command::kRequestAdvertisement;
  }
  bool IsFetchAdvertisementFinished() const {
    return command_ == Command::kRequestAdvertisementFinish;
  }
  bool IsDataConnectionRequest() const {
    return command_ == Command::kRequestDataConnection;
  }
  bool IsDataConnectionReadyResponse() const {
    return command_ == Command::kResponseDataConnectionReady;
  }
  bool IsDataConnectionFailureResponse() const {
    return command_ == Command::kResponseDataConnectionFailure;
  }

 private:
  BleL2capPacket() = default;
  static ByteArray ByteArrayForCommand(Command command, const ByteArray* data);

  Command command_;
  ByteArray service_id_hash_;
  ByteArray advertisement_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_BLE_L2CAP_PACKET_H_
