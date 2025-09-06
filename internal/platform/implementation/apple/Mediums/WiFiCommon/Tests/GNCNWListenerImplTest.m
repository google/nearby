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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWListenerImpl.h"

#import <Network/Network.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWParameters.h"

@interface GNCNWListenerImplTest : XCTestCase
@end

@implementation GNCNWListenerImplTest {
  GNCNWListenerImpl *_listenerImpl;
}

- (void)setUp {
  [super setUp];
  if (@available(iOS 13.0, *)) {
    // Note: GNCBuildNonTLSParameters is not available in tests, so we can't fully initialize.
    // This test is limited to checking basic allocation.
  }
}

// No tests here. The functionality is tested through GNCNWFrameworkServerSocketTest.

// NOTE: Testing the interaction with the Network.framework C functions
// is difficult without a way to mock or intercept C function calls.
// The main test coverage for the listener logic is achieved through
// GNCFakeNWListener in GNCNWFrameworkServerSocketTest.m.

@end
