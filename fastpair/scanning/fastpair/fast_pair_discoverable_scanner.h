// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_DISCOVERABLE_SCANNER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_DISCOVERABLE_SCANNER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fastpair/common/device_metadata.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "internal/base/observer_list.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/mutex.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {
using DiscoverableScannerCallback =
    absl::AnyInvocable<void(FastPairDevice& device)>;
// This class detects Fast Pair 'discoverable' advertisements (see
// https://developers.google.com/nearby/fast-pair/spec#AdvertisingWhenDiscoverable)
// and invokes the |found_callback| when it finds a device within the
// appropriate range.  |lost_callback| will be invoked when that device is lost
// to the bluetooth adapter.
class FastPairDiscoverableScanner : public FastPairScanner::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairDiscoverableScanner> Create(
        FastPairScanner& scanner, DiscoverableScannerCallback found_callback,
        DiscoverableScannerCallback lost_callback,
        SingleThreadExecutor* executor,
        FastPairDeviceRepository* device_repository);

    static void SetFactoryForTesting(Factory* g_test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairDiscoverableScanner> CreateInstance(
        FastPairScanner& scanner, DiscoverableScannerCallback found_callback,
        DiscoverableScannerCallback lost_callback,
        SingleThreadExecutor* executor,
        FastPairDeviceRepository* device_repository) = 0;

   private:
    static Factory* g_test_factory_;
  };

  FastPairDiscoverableScanner(FastPairScanner& scanner,
                              DiscoverableScannerCallback found_callback,
                              DiscoverableScannerCallback lost_callback,
                              SingleThreadExecutor* executor,
                              FastPairDeviceRepository* device_repository);
  FastPairDiscoverableScanner(const FastPairDiscoverableScanner&) = delete;
  FastPairDiscoverableScanner& operator=(const FastPairDiscoverableScanner&) =
      delete;
  ~FastPairDiscoverableScanner();

  // FastPairScanner::Observer
  void OnDeviceFound(const BlePeripheral& peripheral) override;
  void OnDeviceLost(const BlePeripheral& peripheral) override;

 private:
  void OnModelIdRetrieved(const std::string& address,
                          std::optional<absl::string_view> model_id);
  void OnDeviceMetadataRetrieved(std::string address, std::string model_id,
                                 std::optional<DeviceMetadata> device_metadata);
  void NotifyDeviceFound(FastPairDevice& device)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);

  Mutex mutex_;
  FastPairScanner& scanner_;
  DiscoverableScannerCallback found_callback_ ABSL_GUARDED_BY(*executor_);
  DiscoverableScannerCallback lost_callback_ ABSL_GUARDED_BY(*executor_);
  SingleThreadExecutor* executor_;
  absl::flat_hash_map<std::string, FastPairDevice*> notified_devices_
      ABSL_GUARDED_BY(mutex_);
  FastPairDeviceRepository* device_repository_ ABSL_GUARDED_BY(*executor_);
  ObserverList<FastPairScanner::Observer> observer_list_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_DISCOVERABLE_SCANNER_H_
