// Copyright 2022-2023 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_WIFI_INTEL_H_
#define PLATFORM_IMPL_WINDOWS_WIFI_INTEL_H_

// clang-format off
#include <windows.h>
#include <wtypes.h>
#include <cstdint>
// clang-format on

// Intel WIFI PIE headers
#ifndef NO_INTEL_PIE
#include "third_party/intel/pie/include/IntelSdkVersionInfo.h"
#include "third_party/intel/pie/include/PieApiErrors.h"
#include "third_party/intel/pie/include/PieDefinitions.h"
#endif
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
#ifndef NO_INTEL_PIE
using  ::MurocDefs::PINTEL_ADAPTER_LIST_V120;
#endif

// Container of Intel WIFI to utilize Intel PIE SDK API
class WifiIntel {
 public:
  WifiIntel(const WifiIntel&) = delete;
  WifiIntel& operator=(const WifiIntel&) = delete;

  static WifiIntel& GetInstance();
  bool IsValid() const { return intel_wifi_valid_; }
  bool Start();
  void Stop();
  int GetGOChannel();
  bool SetScanFilter(int channel);
  bool ResetScanFilter();


 private:
  // This is a singleton object, for which destructor will never be called.
  // Constructor will be invoked once from Instance() static method.
  // Object is create in-place (with a placement new) to guarantee that
  // destructor is not scheduled for execution at exit.
  WifiIntel() = default;
  ~WifiIntel() = default;

#ifndef NO_INTEL_PIE
  HINSTANCE PIEDllLoader();

  HINSTANCE muroc_api_dll_handle_ = nullptr;
  HADAPTER wifi_adapter_handle_ = 0;
  PINTEL_ADAPTER_LIST_V120 p_all_adapters_ = nullptr;
#endif
  bool intel_wifi_valid_ = false;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_INTEL_H_
