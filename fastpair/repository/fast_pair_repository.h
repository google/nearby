// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/account_key_filter.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/proto/data.proto.h"
#include "fastpair/proto/enum.proto.h"

namespace nearby {
namespace fastpair {
using DeviceMetadataCallback =
    absl::AnyInvocable<void(std::optional<DeviceMetadata> device_metadata)>;
using CheckAccountKeysCallback =
    absl::AnyInvocable<void(std::optional<AccountKey> account_key,
                            std::optional<absl::string_view> model_id)>;
using OperationCallback = absl::AnyInvocable<void(absl::Status status)>;

class FastPairRepository {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnGetUserSavedDevices(
        const proto::OptInStatus& opt_in_status,
        const std::vector<proto::FastPairDevice>& devices) = 0;
  };

  static FastPairRepository* Get();

  // Computes and returns the SHA256 of the concatenation of the given
  // |account_key| and |public_address|.
  static std::string GenerateSha256OfAccountKeyAndMacAddress(
      const AccountKey& account_key, absl::string_view public_address);

  FastPairRepository();
  virtual ~FastPairRepository();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual void GetDeviceMetadata(absl::string_view hex_model_id,
                                 DeviceMetadataCallback callback) = 0;

  // Gets a list of devices saved to the current user's account and the user's
  // opt in status for saving future devices to their account.
  virtual void GetUserSavedDevices() = 0;

  // Stores the given |account_key| for a |device| on the Footprints server.
  virtual void WriteAccountAssociationToFootprints(
      FastPairDevice& device, OperationCallback callback) = 0;

  // Deletes the associated data for a given |account_key|.
  virtual void DeleteAssociatedDeviceByAccountKey(
      const AccountKey& account_key, OperationCallback callback) = 0;

  // Checks all account keys associated with current user's account against the
  // given filter. If a match is found, return the account key.
  virtual void CheckIfAssociatedWithCurrentAccount(
      AccountKeyFilter& account_key_filter,
      CheckAccountKeysCallback callback) = 0;

  // Checks if a device with an address |mac_address| is already saved to
  // the user's account by cross referencing the |mac_address| with any
  // associated account keys.
  virtual void IsDeviceSavedToAccount(absl::string_view mac_address,
                                      OperationCallback callback) = 0;

 protected:
  static void SetInstance(FastPairRepository* instance);
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_REPOSITORY_H_
