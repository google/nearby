// Copyright 2025 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralPeerManager.h"

NS_ASSUME_NONNULL_BEGIN

@class GNSFakePeripheral;

@interface GNSCentralPeerManager (Testing)

@property(nonatomic, nullable) GNSFakePeripheral *testing_peripheral;

@end

NS_ASSUME_NONNULL_END
