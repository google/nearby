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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_NON_DISCOVERABLE_SCANNER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_NON_DISCOVERABLE_SCANNER_H_

#include <memory>
#include <optional>

#include "absl/functional/any_invocable.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/non_discoverable_advertisement.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "internal/base/observer_list.h"
#include "internal/platform/mutex.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {
using NonDiscoverableScannerCallback =
    absl::AnyInvocable<void(FastPairDevice& device)>;
// This class detects Fast Pair 'not discoverable' advertisements (see
// https://developers.google.com/nearby/fast-pair/spec#AdvertisingWhenNotDiscoverable)
// and invokes the |found_callback| when it finds a device within the
// appropriate range.  |lost_callback| will be invoked when that device is lost.
class FastPairNonDiscoverableScanner : public FastPairScanner::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairNonDiscoverableScanner> Create(
        FastPairScanner& scanner, NonDiscoverableScannerCallback found_callback,
        NonDiscoverableScannerCallback lost_callback,
        SingleThreadExecutor* executor,
        FastPairDeviceRepository* device_repository);

    static void SetFactoryForTesting(Factory* g_test_factory);

   protected:
    virtual ~Factory() = default;
    virtual std::unique_ptr<FastPairNonDiscoverableScanner> CreateInstance(
        FastPairScanner& scanner, NonDiscoverableScannerCallback found_callback,
        NonDiscoverableScannerCallback lost_callback,
        SingleThreadExecutor* executor,
        FastPairDeviceRepository* device_repository) = 0;

   private:
    static Factory* g_test_factory_;
  };

  FastPairNonDiscoverableScanner(FastPairScanner& scanner,
                                 NonDiscoverableScannerCallback found_callback,
                                 NonDiscoverableScannerCallback lost_callback,
                                 SingleThreadExecutor* executor,
                                 FastPairDeviceRepository* device_repository);
  FastPairNonDiscoverableScanner(const FastPairNonDiscoverableScanner&) =
      delete;
  FastPairNonDiscoverableScanner& operator=(
      const FastPairNonDiscoverableScanner&) = delete;
  ~FastPairNonDiscoverableScanner();

  // FastPairScanner::Observer
  void OnDeviceFound(const BlePeripheral& peripheral) override;
  void OnDeviceLost(const BlePeripheral& peripheral) override;

 private:
  void OnAdvertisementParsed(
      absl::string_view address,
      std::optional<NonDiscoverableAdvertisement> advertisement);
  void OnAccountKeyFilterCheckResult(
      absl::string_view address,
      std::optional<NonDiscoverableAdvertisement> advertisement,
      std::optional<AccountKey> account_key,
      std::optional<absl::string_view> model_id);
  void OnDeviceMetadataRetrieved(
      absl::string_view address,
      std::optional<NonDiscoverableAdvertisement> advertisement,
      absl::string_view model_id, AccountKey account_key,
      std::optional<DeviceMetadata> device_metadata);
  void NotifyDeviceFound(FastPairDevice& device)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);

  Mutex mutex_;
  FastPairScanner& scanner_;
  NonDiscoverableScannerCallback found_callback_ ABSL_GUARDED_BY(*executor_);
  NonDiscoverableScannerCallback lost_callback_ ABSL_GUARDED_BY(*executor_);
  SingleThreadExecutor* executor_;
  absl::flat_hash_map<std::string, FastPairDevice*> notified_devices_
      ABSL_GUARDED_BY(mutex_);
  FastPairDeviceRepository* device_repository_ ABSL_GUARDED_BY(*executor_);
  ObserverList<FastPairScanner::Observer> observer_list_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_NON_DISCOVERABLE_SCANNER_H_
