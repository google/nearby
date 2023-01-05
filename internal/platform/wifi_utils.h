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

#ifndef PLATFORM_BASE_WIFI_UTILS_H_
#define PLATFORM_BASE_WIFI_UTILS_H_

#include <string>

#include "internal/platform/implementation/wifi.h"

namespace nearby {

using ::nearby::api::WifiBandType;

class WifiUtils {
 public:
  static constexpr int kUnspecified = -1;
  static constexpr int kBand24GhzFirstChNum = 1;
  static constexpr int kBand24GhzLastChNum = 14;
  static constexpr int kBand24GhzStartFreqMhz = 2412;
  static constexpr int kBand24GhzEndFreqMhz = 2484;
  static constexpr int kBand5GhzFirstChNum = 32;
  static constexpr int kBand5GhzLastChNum = 177;
  static constexpr int kBand5GhzStartFreqMhz = 5160;
  static constexpr int kBand5GhzEndFreqMhz = 5885;
  static constexpr int kBand6GhzFirstChNum = 1;
  static constexpr int kBand6GhzLastChNum = 233;
  static constexpr int kBand6GhzStartFreqMhz = 5955;
  static constexpr int kBand6GhzEndFreqMhz = 7115;
  static constexpr int kBand6GhzPscStartMhz = 5975;
  static constexpr int kBand6GhzPscStepSizeMhz = 80;
  static constexpr int kBand6GhzOpClass136Ch2FreqMhz = 5935;
  static constexpr int kBand60GhzFirstChNum = 1;
  static constexpr int kBand60GhzLastChNum = 6;
  static constexpr int kBand60GhzStartFreqMhz = 58320;
  static constexpr int kBand60GhzEndFreqMhz = 70200;

  static int ConvertChannelToFrequencyMhz(int channel, WifiBandType band_type);
  static int ConvertFrequencyMhzToChannel(int freq_mhz);

  static bool ValidateIPV4(std::string ipv4);
};

}  // namespace nearby

#endif  // PLATFORM_BASE_WIFI_UTILS_H_
