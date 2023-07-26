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

#include "fastpair/repository/fast_pair_repository_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/proto/data.proto.h"
#include "fastpair/proto/enum.proto.h"
#include "fastpair/proto/proto_builder.h"
#include "internal/platform/logging.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

namespace {
// This forget pattern is defined in the Android codebase as FORGET_PREFIX_BYTE
// and FORGET_PREFIX_LENGTH_IN_BYTES. Currently, those values evaluate to the
// string of bytes defined below, which is used as the prefix for the sha256
// field of the device.
constexpr absl::string_view kForgetPattern = "\xf0\xf0\xf0\xf0";

// For all intents and purposes, a device that has the "Forget pattern" is no
// longer associated to the user's account, and should be treated as removed.
bool DoesDeviceHaveForgetPattern(const proto::FastPairDevice& device) {
  // The device info is modified to have no account key upon removal from
  // Fast Pair Saved Devices
  if (device.account_key().empty() ||
      device.sha256_account_key_public_address().empty()) {
    return true;
  }

  // To match Android behavior, we check if the SHA256 of a device begins with
  // the Forget pattern, defined in Android Fast Pair code. When a device is
  // forgotten from Android Bluetooth Settings, the SHA256 hash is modified to
  // contain this pattern.
  return (device.sha256_account_key_public_address().compare(
              0, kForgetPattern.length(), kForgetPattern) == 0);
}

// Checks if the mac address of a FastPairDevice is the same as the given
// |mac_address| by checking if the SHA256 from the given |device| equals to
// SHA256(concat(account_key of |device|, |mac_address|)).
bool IsDeviceSha256Matched(const proto::FastPairDevice& device,
                           absl::string_view mac_address) {
  if (DoesDeviceHaveForgetPattern(device)) {
    return false;
  }

  return device.sha256_account_key_public_address() ==
         FastPairRepository::GenerateSha256OfAccountKeyAndMacAddress(
             AccountKey(device.account_key()), mac_address);
}
}  // namespace

FastPairRepositoryImpl::FastPairRepositoryImpl(FastPairClient* fast_pair_client)
    : fast_pair_client_(fast_pair_client) {}

void FastPairRepositoryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FastPairRepositoryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FastPairRepositoryImpl::GetDeviceMetadata(
    absl::string_view hex_model_id, DeviceMetadataCallback callback) {
  NEARBY_LOGS(INFO) << __func__ << " with model id= " << hex_model_id;
  executor_.Execute(
      "Get Device Metadata", [this, hex_model_id = std::string(hex_model_id),
                              callback = std::move(callback)]() mutable {
        NEARBY_LOGS(INFO) << __func__ << ": Start to get devic metadata.";
        proto::GetObservedDeviceRequest request;
        int64_t device_id;
        CHECK(absl::SimpleHexAtoi(hex_model_id, &device_id));
        request.set_device_id(device_id);
        request.set_mode(proto::GetObservedDeviceRequest::MODE_RELEASE);
        absl::StatusOr<proto::GetObservedDeviceResponse> response =
            fast_pair_client_->GetObservedDevice(request);
        if (response.ok()) {
          NEARBY_LOGS(WARNING) << "Got GetObservedDeviceResponse from backend.";
          metadata_cache_[hex_model_id] =
              std::make_unique<DeviceMetadata>(response.value());
          // TODO(b/289139378) : save device's metadata in local cache.
          callback(*metadata_cache_[hex_model_id]);
        } else {
          NEARBY_LOGS(WARNING)
              << "Failed to get GetObservedDeviceResponse from backend.";
          callback(std::nullopt);
        }
      });
}

void FastPairRepositoryImpl::WriteAccountAssociationToFootprints(
    FastPairDevice& device, OperationCallback callback) {
  proto::UserWriteDeviceRequest request;
  auto* fast_pair_info = request.mutable_fast_pair_info();
  BuildFastPairInfo(fast_pair_info, device);
  executor_.Execute(
      "Write associated device", [this, request = std::move(request),
                                  callback = std::move(callback)]() mutable {
        NEARBY_LOGS(INFO)
            << __func__
            << ": Start to write account associated device to footprints.";
        absl::StatusOr<proto::UserWriteDeviceResponse> response =
            fast_pair_client_->UserWriteDevice(request);
        if (response.ok()) {
          NEARBY_LOGS(INFO)
              << __func__ << "Got GetWriteDeviceResponse from backend.";
          std::move(callback)(absl::OkStatus());
        } else {
          NEARBY_LOGS(WARNING)
              << __func__
              << "Failed to get GetWriteDeviceResponse from backend.";
          std::move(callback)(response.status());
        }
      });
}

void FastPairRepositoryImpl::DeleteAssociatedDeviceByAccountKey(
    const AccountKey& account_key, OperationCallback callback) {
  std::string hex_string = absl::BytesToHexString(account_key.GetAsBytes());
  absl::AsciiStrToUpper(&hex_string);
  executor_.Execute(
      "Delete associated device",
      [this, hex_account_key = std::move(hex_string),
       callback = std::move(callback)]() mutable {
        NEARBY_LOGS(INFO)
            << __func__
            << ": Start to delete account associated device from footprints";
        proto::UserDeleteDeviceRequest request;
        request.set_hex_account_key(hex_account_key);
        absl::StatusOr<proto::UserDeleteDeviceResponse> response =
            fast_pair_client_->UserDeleteDevice(request);
        if (response.ok()) {
          if (response->success()) {
            NEARBY_LOGS(INFO)
                << __func__ << "Successfully deleted associated device.";
            std::move(callback)(absl::OkStatus());
          } else {
            NEARBY_LOGS(WARNING) << __func__ << "Failed to delete device.";
            std::move(callback)(
                absl::InternalError("Failed to delete device."));
          }
        } else {
          NEARBY_LOGS(WARNING)
              << __func__
              << "Failed to get UserDeleteDeviceResponse from backend.";
          std::move(callback)(response.status());
        }
      });
}

void FastPairRepositoryImpl::GetUserSavedDevices() {
  executor_.Execute("Get associated devices", [this]() mutable {
    NEARBY_LOGS(INFO) << __func__
                      << ": Start to get all account associated devices.";
    proto::UserReadDevicesRequest request;
    absl::StatusOr<proto::UserReadDevicesResponse> response =
        fast_pair_client_->UserReadDevices(request);
    if (!response.ok()) {
      NEARBY_LOGS(WARNING)
          << __func__ << "Failed to get UserReadDevicesResponse from backend.";
      return;
    }
    NEARBY_LOGS(INFO) << __func__
                      << "Got UserReadDevicesResponse from backend.";
    proto::OptInStatus opt_in_status =
        proto::OptInStatus::OPT_IN_STATUS_UNKNOWN;
    std::vector<proto::FastPairDevice> saved_devices;
    for (const auto& info : response->fast_pair_info()) {
      if (info.has_opt_in_status()) {
        opt_in_status = info.opt_in_status();
      }
      // We have to check that the devices in Footprints don't use the
      // "forget pattern" which Android uses in some cases to mark a device
      // as removed from the user's account.
      if (!info.has_device() || DoesDeviceHaveForgetPattern(info.device())) {
        continue;
      }
      saved_devices.push_back(info.device());
    }
    NEARBY_LOGS(INFO) << __func__ << ": Got " << saved_devices.size()
                      << " saved devices.";
    // TODO(b/289139378) : save device's in local cache.
    for (auto& observer : observers_.GetObservers()) {
      observer->OnGetUserSavedDevices(opt_in_status, saved_devices);
    }
  });
}

void FastPairRepositoryImpl::CheckIfAssociatedWithCurrentAccount(
    AccountKeyFilter& account_key_filter, CheckAccountKeysCallback callback) {
  executor_.Execute("Check if associated.", [this,
                                             account_key_filter =
                                                 std::move(account_key_filter),
                                             callback = std::move(
                                                 callback)]() mutable {
    NEARBY_LOGS(INFO) << __func__
                      << ": Start to check if associated with current account.";
    proto::UserReadDevicesRequest request;
    absl::StatusOr<proto::UserReadDevicesResponse> response =
        fast_pair_client_->UserReadDevices(request);
    if (response.ok()) {
      for (const auto& info : response->fast_pair_info()) {
        if (!info.has_device()) {
          continue;
        }
        AccountKey account_key(info.device().account_key());
        if (!account_key_filter.IsPossiblyInSet(account_key)) {
          continue;
        }
        proto::StoredDiscoveryItem device;
        if (device.ParseFromString(info.device().discovery_item_bytes())) {
          NEARBY_LOGS(INFO)
              << "Account key matched with a paired device: " << device.title();
          std::move(callback)(account_key, device.id());
          return;
        }
      }
    }
    NEARBY_LOGS(INFO) << "Account key does not match any paired devices.";
    std::move(callback)(std::nullopt, std::nullopt);
  });
}

void FastPairRepositoryImpl::IsDeviceSavedToAccount(
    absl::string_view mac_address, OperationCallback callback) {
  executor_.Execute(
      "Check is device saved to account.",
      [this, mac_address = std::string(mac_address),
       callback = std::move(callback)]() mutable {
        NEARBY_LOGS(INFO) << __func__
                          << ": Start to check is device saved to account.";
        proto::UserReadDevicesRequest request;
        absl::StatusOr<proto::UserReadDevicesResponse> response =
            fast_pair_client_->UserReadDevices(request);
        if (!response.ok()) {
          NEARBY_LOGS(WARNING)
              << __func__
              << "Failed to get UserDeleteDeviceResponse from backend.";
          std::move(callback)(response.status());
          return;
        }
        for (const auto& info : response->fast_pair_info()) {
          if (info.has_device() &&
              IsDeviceSha256Matched(info.device(), mac_address)) {
            NEARBY_LOGS(VERBOSE)
                << __func__ << ": found a SHA256 match for device at address = "
                << mac_address;
            std::move(callback)(absl::OkStatus());
            return;
          }
        }
        std::move(callback)(absl::NotFoundError("Device " + mac_address +
                                                " is not saved to account."));
      });
}

}  // namespace fastpair
}  // namespace nearby
