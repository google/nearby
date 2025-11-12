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

@interface GNCNWListenerImplTest : XCTestCase
@end

@implementation GNCNWListenerImplTest

- (void)testInitWithParameters {
  nw_parameters_t parameters = nw_parameters_create_secure_tcp(NW_PARAMETERS_DEFAULT_CONFIGURATION,
                                                               NW_PARAMETERS_DEFAULT_CONFIGURATION);
  GNCNWListenerImpl *listener = [[GNCNWListenerImpl alloc] initWithParameters:parameters];
  XCTAssertNotNil(listener);
}

- (void)testInitWithPortAndParameters {
  nw_parameters_t parameters = nw_parameters_create_secure_tcp(NW_PARAMETERS_DEFAULT_CONFIGURATION,
                                                               NW_PARAMETERS_DEFAULT_CONFIGURATION);
  GNCNWListenerImpl *listener = [[GNCNWListenerImpl alloc] initWithPort:"1234"
                                                             parameters:parameters];
  XCTAssertNotNil(listener);
}

@end
