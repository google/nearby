// Copyright 2020 Google LLC
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
#include <map>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace cal {

using ByteArray = location::nearby::ByteArray;
using Exception = location::nearby::Exception;
using InputStream = location::nearby::InputStream;
using OutputStream = location::nearby::OutputStream;

enum class AdvertiseMode {
  kUnknown = 0,
  kLowPower = 1,
  kBalanced = 2,
  kLowLatency = 3,
};

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

struct BleAdvertisementData {
  AdvertiseSettings advertise_settings;
  // service UUID to advertise bytes map:16-bit service UUID - service Data
  //
  // Data type value : 0x16
  // if platform can't advertise data from this key (reaonly in iOS), then (iOS)
  // should advertise data via LocalName data type. It means the service_uuid
  // here is useless for iOS platform.
  std::map<std::string, location::nearby::ByteArray> service_data_map;
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
