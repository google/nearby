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

#include "fastpair/repository/fake_fast_pair_repository.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/account_key_filter.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/proto/data.proto.h"
#include "fastpair/proto/enum.proto.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/repository/fast_pair_repository.h"

namespace nearby {
namespace fastpair {
void FakeFastPairRepository::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeFastPairRepository::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeFastPairRepository::SetFakeMetadata(absl::string_view hex_model_id,
                                             proto::Device metadata) {
  proto::GetObservedDeviceResponse response;
  *response.mutable_device() = metadata;
  data_[hex_model_id] = std::make_unique<DeviceMetadata>(response);
}

void FakeFastPairRepository::ClearFakeMetadata(absl::string_view hex_model_id) {
  data_.erase(hex_model_id);
}

void FakeFastPairRepository::SetResultOfCheckIfAssociatedWithCurrentAccount(
    std::optional<AccountKey> account_key,
    std::optional<absl::string_view> model_id) {
  account_key_ = std::move(account_key);
  model_id_ = std::move(model_id);
}

void FakeFastPairRepository::SetResultOfWriteAccountAssociationToFootprints(
    absl::Status status) {
  write_account_association_to_footprints_ = status;
}

void FakeFastPairRepository::SetResultOfDeleteAssociatedDeviceByAccountKey(
    absl::Status status) {
  deleted_associated_device_ = status;
}

void FakeFastPairRepository::SetResultOfIsDeviceSavedToAccount(
    absl::Status status) {
  is_device_saved_to_account_ = status;
}

void FakeFastPairRepository::GetDeviceMetadata(
    absl::string_view hex_model_id, DeviceMetadataCallback callback) {
  executor_.Execute([this, callback = std::move(callback),
                     hex_model_id = std::string(hex_model_id)]() mutable {
    if (data_.contains(hex_model_id)) {
      std::move(callback)(*data_[hex_model_id]);
    } else {
      std::move(callback)(std::nullopt);
    }
  });
}

void FakeFastPairRepository::GetUserSavedDevices() {
  proto::OptInStatus opt_in_status = proto::OptInStatus::OPT_IN_STATUS_UNKNOWN;
  std::vector<proto::FastPairDevice> saved_devices;
  for (auto& observer : observers_.GetObservers()) {
    observer->OnGetUserSavedDevices(opt_in_status, saved_devices);
  }
}

void FakeFastPairRepository::WriteAccountAssociationToFootprints(
    FastPairDevice& device, OperationCallback callback) {
  executor_.Execute([callback = std::move(callback), this]() mutable {
    callback(write_account_association_to_footprints_);
  });
}

void FakeFastPairRepository::DeleteAssociatedDeviceByAccountKey(
    const AccountKey& account_key, OperationCallback callback) {
  executor_.Execute([callback = std::move(callback), this]() mutable {
    callback(deleted_associated_device_);
  });
}

void FakeFastPairRepository::CheckIfAssociatedWithCurrentAccount(
    AccountKeyFilter& account_key_filter, CheckAccountKeysCallback callback) {
  executor_.Execute([this, callback = std::move(callback)]() mutable {
    std::move(callback)(account_key_, model_id_);
  });
}

void FakeFastPairRepository::IsDeviceSavedToAccount(
    absl::string_view mac_address, OperationCallback callback) {
  executor_.Execute([callback = std::move(callback), this]() mutable {
    callback(is_device_saved_to_account_);
  });
}

std::unique_ptr<FakeFastPairRepository> FakeFastPairRepository::Create(
    absl::string_view model_id, absl::string_view public_anti_spoof_key) {
  proto::Device metadata;
  auto repository = std::make_unique<FakeFastPairRepository>();
  if (public_anti_spoof_key.empty()) {
    // Missing ASK is fine for V1 devices.
  } else if (public_anti_spoof_key.length() == kPublicKeyByteSize) {
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(
        public_anti_spoof_key);
  } else {
    std::string decoded_key;
    absl::Base64Unescape(public_anti_spoof_key, &decoded_key);
    CHECK_EQ(decoded_key.length(), kPublicKeyByteSize);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
  }
  repository->SetFakeMetadata(model_id, metadata);
  return repository;
}
}  // namespace fastpair
}  // namespace nearby
