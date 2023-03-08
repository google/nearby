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

#import "connections/swift/NearbyCoreAdapter/Sources/GNCSupportedMediums+CppConversions.h"

#include "connections/medium_selector.h"

#import "connections/swift/NearbyCoreAdapter/Sources/GNCStrategy+Internal.h"

using ::nearby::connections::BooleanMediumSelector;

@implementation GNCSupportedMediums (CppConversions)

- (BooleanMediumSelector)toCpp {
  BooleanMediumSelector selector;

  selector.ble = self.ble;
  selector.bluetooth = self.bluetooth;
  selector.web_rtc = self.webRTC;
  selector.wifi_lan = self.wifiLAN;
  selector.wifi_hotspot = self.wifiHotspot;
  selector.wifi_direct = self.wifiDirect;

  return selector;
}

@end
