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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_DISCOVERABLE_SCANNER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_DISCOVERABLE_SCANNER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/repository/device_metadata.h"
#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "internal/base/observer_list.h"
#include "internal/platform/bluetooth_adapter.h"

namespace nearby {
namespace fastpair {

class FastPairDiscoverableScannerImpl : public FastPairDiscoverableScanner,
                                        public FastPairScanner::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairDiscoverableScanner> Create(
        std::shared_ptr<FastPairScanner> scanner,
        std::shared_ptr<BluetoothAdapter> adapter,
        DeviceCallback found_callback, DeviceCallback lost_callback);

    static void SetFactoryForTesting(Factory* g_test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairDiscoverableScanner> CreateInstance(
        std::shared_ptr<FastPairScanner> scanner,
        std::shared_ptr<BluetoothAdapter> adapter,
        DeviceCallback found_callback, DeviceCallback lost_callback) = 0;

   private:
    static Factory* g_test_factory_;
  };

  FastPairDiscoverableScannerImpl(std::shared_ptr<FastPairScanner> scanner,
                                  std::shared_ptr<BluetoothAdapter> adapter,
                                  DeviceCallback found_callback,
                                  DeviceCallback lost_callback);
  FastPairDiscoverableScannerImpl(const FastPairDiscoverableScannerImpl&) =
      delete;
  FastPairDiscoverableScannerImpl& operator=(
      const FastPairDiscoverableScannerImpl&) = delete;
  ~FastPairDiscoverableScannerImpl() override = default;

  // FastPairScanner::Observer
  void OnDeviceFound(const BlePeripheral& peripheral) override;
  void OnDeviceLost(const BlePeripheral& peripheral) override;

 private:
  void OnModelIdRetrieved(const std::string& address,
                          std::optional<absl::string_view> model_id);
  void OnDeviceMetadataRetrieved(const std::string& address,
                                 const std::string model_id,
                                 DeviceMetadata& device_metadata);
  void NotifyDeviceFound(FastPairDevice& device);

  std::shared_ptr<FastPairScanner> scanner_;
  std::shared_ptr<BluetoothAdapter> adapter_;
  DeviceCallback found_callback_;
  DeviceCallback lost_callback_;
  absl::flat_hash_map<std::string, FastPairDevice*> notified_devices_;
  absl::flat_hash_map<std::string, int> model_id_parse_attempts_;
  ObserverList<FastPairScanner::Observer> observer_list_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_DISCOVERABLE_SCANNER_IMPL_H_
