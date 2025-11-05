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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowserImpl.h"

#import <Network/Network.h>
#import <XCTest/XCTest.h>

@interface GNCNWBrowserImplTest : XCTestCase
@end

@implementation GNCNWBrowserImplTest

- (void)testInit {
  GNCNWBrowserImpl *browser = [[GNCNWBrowserImpl alloc] init];
  XCTAssertNotNil(browser);
}

// NOTE: The methods in GNCNWBrowserImpl call C functions from the Network.framework
// (e.g., nw_browser_create, nw_browser_start, nw_browser_cancel).
// OCMock cannot directly mock these C functions. To test these interactions,
// a lower-level interception method or a different testing strategy would be required.
// We can only test the basic initialization here.

@end
