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

#include "platform/base/byte_array.h"
#include "platform/base/exception.h"
#include "platform/base/input_stream.h"
#include "platform/base/listeners.h"
#include "platform/base/output_stream.h"

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
  PowerMode poser_mode;
  int timeout;
  bool is_connectable;
  std::string local_name;
};

struct BleAdvertisementData {
  AdvertiseSettings advertise_settings;
  // service UUID to advertise bytes map
  std::map<std::string, ByteArray> service_data_map;
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
