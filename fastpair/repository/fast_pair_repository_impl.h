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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_REPOSITORY_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_REPOSITORY_IMPL_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "fastpair/server_access/fast_pair_client.h"
#include "internal/base/observer_list.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

class FastPairRepositoryImpl : public FastPairRepository {
 public:
  explicit FastPairRepositoryImpl(FastPairClient* fast_pair_client);

  FastPairRepositoryImpl(const FastPairRepositoryImpl&) = delete;
  FastPairRepositoryImpl& operator=(const FastPairRepositoryImpl&) = delete;
  ~FastPairRepositoryImpl() override = default;

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
  // A thread for running blocking tasks.
  SingleThreadExecutor executor_;
  FastPairClient* fast_pair_client_;
  absl::flat_hash_map<std::string, std::unique_ptr<DeviceMetadata>>
      metadata_cache_;
  ObserverList<FastPairRepository::Observer> observers_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_REPOSITORY_IMPL_H_
