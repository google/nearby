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

#include "internal/platform/wifi_utils.h"

#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"

namespace nearby {

// Utility function to convert channel number to frequency in MHz
// @param channel to convert
// @return center frequency in Mhz of the channel, return "kUnspecified" if no
// match
// Add band support later
int WifiUtils::ConvertChannelToFrequencyMhz(int channel, WifiBandType band) {
  if (band == WifiBandType::kUnknown || band == WifiBandType::kBand24Ghz ||
      band == WifiBandType::kBand5Ghz) {
    if (channel == 14) {
      return 2484;
    } else if (channel >= kBand24GhzFirstChNum &&
               channel <= kBand24GhzLastChNum) {
      return ((channel - kBand24GhzFirstChNum) * 5) + kBand24GhzStartFreqMhz;
    } else if (channel >= kBand5GhzFirstChNum &&
               channel <= kBand5GhzLastChNum) {
      return ((channel - kBand5GhzFirstChNum) * 5) + kBand5GhzStartFreqMhz;
    } else {
      return kUnspecified;
    }
  }

  if (band == WifiBandType::kBand6Ghz) {
    if (channel >= kBand6GhzFirstChNum && channel <= kBand6GhzLastChNum) {
      if (channel == 2) {
        return kBand6GhzOpClass136Ch2FreqMhz;
      }
      return ((channel - kBand6GhzFirstChNum) * 5) + kBand6GhzStartFreqMhz;
    } else {
      return kUnspecified;
    }
  }

  if (band == WifiBandType::kBand60Ghz) {
    if (channel >= kBand60GhzFirstChNum && channel <= kBand60GhzLastChNum) {
      return ((channel - kBand60GhzFirstChNum) * 2160) + kBand60GhzStartFreqMhz;
    } else {
      return kUnspecified;
    }
  }

  return kUnspecified;
}

// Utility function to convert frequency in MHz to channel number
// @param freqMhz frequency in MHz
// @return channel number associated with given frequency, return "kUnspecified"
// if no match
int WifiUtils::ConvertFrequencyMhzToChannel(int freq_mhz) {
  // Special case
  if (freq_mhz == kBand24GhzEndFreqMhz) {
    return 14;
  } else if (freq_mhz >= kBand24GhzStartFreqMhz &&
             freq_mhz <= kBand24GhzEndFreqMhz) {
    return (freq_mhz - kBand24GhzStartFreqMhz) / 5 + kBand24GhzFirstChNum;
  } else if (freq_mhz >= kBand5GhzStartFreqMhz &&
             freq_mhz <= kBand5GhzEndFreqMhz) {
    return (freq_mhz - kBand5GhzStartFreqMhz) / 5 + kBand5GhzFirstChNum;
  } else if (freq_mhz >= kBand6GhzStartFreqMhz &&
             freq_mhz <= kBand6GhzEndFreqMhz) {
    if (freq_mhz == kBand6GhzOpClass136Ch2FreqMhz) {
      return 2;
    }
    return (freq_mhz - kBand6GhzStartFreqMhz) / 5 + kBand6GhzFirstChNum;
  } else if (freq_mhz >= kBand60GhzStartFreqMhz &&
             freq_mhz <= kBand60GhzEndFreqMhz) {
    return (freq_mhz - kBand60GhzStartFreqMhz) / 2160 + kBand60GhzFirstChNum;
  }

  return kUnspecified;
}

// Function to validate an IP address
bool WifiUtils::ValidateIPV4(std::string ipv4) {
  int result;
  // split the string into tokens
  std::vector<absl::string_view> list = absl::StrSplit(ipv4, '.');

  // if the token size is not equal to four
  if (list.size() != 4) {
    return false;
  }

  // validate each token
  for (absl::string_view str : list) {
    // verify that the string is a number or not, and the numbers are in the
    // valid range
    if (!absl::SimpleAtoi(str, &result)) return false;
    if (result > 255 || result < 0) return false;
  }

  return true;
}

}  // namespace nearby
