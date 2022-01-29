// Copyright 2021 Google LLC
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

#ifndef CAL_BASE_TYPES_H_
#define CAL_BASE_TYPES_H_

// TODO(hais) relocate base def class accordingly.
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace cal {

enum class AdvertiseMode {
  kUnknown = 0,
  kLowPower = 1,
  kBalanced = 2,
  kLowLatency = 3,
};

// Coarse representation of power settings throughout all BLE operations.
enum class PowerMode {
  kUnknown = 0,
  kUltraLow = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,
};

struct AdvertiseSettings {
  AdvertiseMode advertise_mode;
  PowerMode power_mode;
  int timeout;
  bool is_connectable;
};

// https://developer.android.com/reference/android/bluetooth/le/AdvertiseData
//
// Bundle of data found in a BLE advertisement.
//
// All service UUIDs will conform to the 16-bit Bluetooth base UUID,
// 0000xxxx-0000-1000-8000-00805F9B34FB. This makes it possible to store two
// byte service UUIDs in the advertisement.
struct BleAdvertisementData {
  using TxPowerLevel = std::int8_t;

  static constexpr TxPowerLevel kUnspecifiedTxPowerLevel =
      std::numeric_limits<TxPowerLevel>::min();

  bool is_connectable;

  // If tx_power_level is not set to kUnspecifiedTxPowerLevel, platform
  // implementer needs to set the TxPowerLevel.
  TxPowerLevel tx_power_level;

  // If the set is not empty, the platform implementer needs to add the
  // service_uuids in the advertisement data.
  absl::flat_hash_set<std::string> service_uuids;

  // Maps service UUIDs to their service data.
  //
  // Note if platform can't advertise data from Data type (0x16)
  // (reaonly in iOS), then (iOS) should advertise data via LocalName data type
  // (0x08).
  // It means the iOS should take the first index of service_data as the data
  // for LocalName type.
  absl::flat_hash_map<std::string, location::nearby::ByteArray> service_data;
};

enum class ScanMode {
  kUnknown = 0,
  kLowPower,
  kBalanced,
  kLowLatency,
  // kOpportunistic not supported
};

struct ScanSettings {
  ScanMode scan_mode;
  // Do we need reportDelay, phy, legacy?
};

}  // namespace cal
}  // namespace nearby

#endif  // CAL_BASE_TYPES_H_
