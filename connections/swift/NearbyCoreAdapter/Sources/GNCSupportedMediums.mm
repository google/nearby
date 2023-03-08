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

#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCSupportedMediums.h"

@implementation GNCSupportedMediums

- (instancetype)initWithAllMediumsEnabled {
  return [self initWithBluetooth:YES ble:YES webRTC:YES wifiLAN:YES wifiHotspot:YES wifiDirect:YES];
}

- (instancetype)initWithBluetooth:(BOOL)bluetooth
                              ble:(BOOL)ble
                           webRTC:(BOOL)webRTC
                          wifiLAN:(BOOL)wifiLAN
                      wifiHotspot:(BOOL)wifiHotspot
                       wifiDirect:(BOOL)wifiDirect {
  self = [super init];
  if (self) {
    _bluetooth = bluetooth;
    _ble = ble;
    _webRTC = webRTC;
    _wifiLAN = wifiLAN;
    _wifiHotspot = wifiHotspot;
    _wifiDirect = wifiDirect;
  }
  return self;
}

@end
