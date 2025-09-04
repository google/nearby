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

#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotMedium.h"

#import <CoreLocation/CoreLocation.h>
#import <NetworkExtension/NetworkExtension.h>

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"

NS_ASSUME_NONNULL_BEGIN

// Expose for testing
@interface GNCHotspotMedium (Testing)
- (nullable GNCHotspotSocket *)connectToEndpoint:(nw_endpoint_t)endpoint
                               includePeerToPeer:(BOOL)includePeerToPeer
                                    cancelSource:(nullable dispatch_source_t)cancelSource
                                           error:(NSError *_Nullable *_Nullable)error;
@end

@interface GNCHotspotMediumTests : XCTestCase
@end

// TODO: b/434870979 - Use fake LocationManager and NWFramework instead of mocking.
@implementation GNCHotspotMediumTests {
  GNCHotspotMedium *_hotspotMedium;
}

- (void)setUp {
  [super setUp];
  _hotspotMedium = [[GNCHotspotMedium alloc] initWithQueue:dispatch_get_main_queue()];
}

- (void)testInit {
  XCTAssertNotNil(_hotspotMedium);
}

- (void)testDefaultInit {
  GNCHotspotMedium *medium = [[GNCHotspotMedium alloc] init];
  XCTAssertNotNil(medium);
}


@end

NS_ASSUME_NONNULL_END