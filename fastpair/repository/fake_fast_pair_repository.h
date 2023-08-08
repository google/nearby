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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAKE_FAST_PAIR_REPOSITORY_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAKE_FAST_PAIR_REPOSITORY_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "internal/base/observer_list.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {
class FakeFastPairRepository : public FastPairRepository {
 public:
  FakeFastPairRepository() = default;
  FakeFastPairRepository(const FakeFastPairRepository&) = delete;
  FakeFastPairRepository& operator=(const FakeFastPairRepository&) = delete;
  ~FakeFastPairRepository() override = default;

  static std::unique_ptr<FakeFastPairRepository> Create(
      absl::string_view model_id, absl::string_view public_anti_spoof_key);

  void SetFakeMetadata(absl::string_view hex_model_id, proto::Device metadata);
  void ClearFakeMetadata(absl::string_view hex_model_id);
  void SetResultOfWriteAccountAssociationToFootprints(absl::Status status);
  void SetResultOfDeleteAssociatedDeviceByAccountKey(absl::Status status);
  void SetResultOfCheckIfAssociatedWithCurrentAccount(
      std::optional<AccountKey> account_key,
      std::optional<absl::string_view> model_id);
  void SetResultOfIsDeviceSavedToAccount(absl::Status status);

  // FastPairRepository::
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void GetDeviceMetadata(absl::string_view hex_model_id,
                         DeviceMetadataCallback callback) override;

  void GetUserSavedDevices() override;

  void WriteAccountAssociationToFootprints(FastPairDevice& device,
                                           OperationCallback callback) override;

  void DeleteAssociatedDeviceByAccountKey(const AccountKey& account_key,
                                          OperationCallback callback) override;

  void CheckIfAssociatedWithCurrentAccount(
      AccountKeyFilter& account_key_filter,
      CheckAccountKeysCallback callback) override;

  void IsDeviceSavedToAccount(absl::string_view mac_address,
                              OperationCallback callback) override;

 private:
  absl::flat_hash_map<std::string, std::unique_ptr<DeviceMetadata>> data_;

  // Results of CheckIfAssociatedWithCurrentAccount
  std::optional<AccountKey> account_key_;
  std::optional<absl::string_view> model_id_;
  // Results of WriteAccountAssociationToFootprints
  absl::Status write_account_association_to_footprints_;
  // Results of DeleteAssociatedDeviceByAccountKey
  absl::Status deleted_associated_device_;
  // Results of IsDeviceSavedToAccount
  absl::Status is_device_saved_to_account_;
  ObserverList<FastPairRepository::Observer> observers_;
  SingleThreadExecutor executor_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAKE_FAST_PAIR_REPOSITORY_H_
