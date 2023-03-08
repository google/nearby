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

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface GNCSupportedMediums : NSObject
- (instancetype)init NS_UNAVAILABLE;

/** Initializes with all mediums being supported. */
- (instancetype)initWithAllMediumsEnabled;

/** Initializes, setting properties to the provided values. */
- (instancetype)initWithBluetooth:(BOOL)bluetooth
                              ble:(BOOL)ble
                           webRTC:(BOOL)webRTC
                          wifiLAN:(BOOL)wifiLAN
                      wifiHotspot:(BOOL)wifiHotspot
                       wifiDirect:(BOOL)wifiDirect NS_DESIGNATED_INITIALIZER;

@property(nonatomic) BOOL bluetooth;
@property(nonatomic) BOOL ble;
@property(nonatomic) BOOL webRTC;
@property(nonatomic) BOOL wifiLAN;
@property(nonatomic) BOOL wifiHotspot;
@property(nonatomic) BOOL wifiDirect;

@end

NS_ASSUME_NONNULL_END
