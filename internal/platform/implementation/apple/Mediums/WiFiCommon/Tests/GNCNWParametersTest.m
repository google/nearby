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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWParameters.h"

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

@interface GNCNWParametersTest : XCTestCase
@end

@implementation GNCNWParametersTest

- (void)testNonTLSParameters {
  nw_parameters_t params = GNCBuildNonTLSParameters(YES);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(params));
}

- (void)testNonTLSParametersWithNoPeerToPeer {
  nw_parameters_t params = GNCBuildNonTLSParameters(NO);
  XCTAssertFalse(nw_parameters_get_include_peer_to_peer(params));
}

- (void)testTLSParametersWithEmptyPSK {
  nw_parameters_t params =
      GNCBuildTLSParameters([@"" dataUsingEncoding:NSUTF8StringEncoding],
                            [@"nearby" dataUsingEncoding:NSUTF8StringEncoding], YES);
  XCTAssertNil(params);
}

- (void)testTLSParameters {
  nw_parameters_t params =
      GNCBuildTLSParameters([@"12345678" dataUsingEncoding:NSUTF8StringEncoding],
                            [@"nearby" dataUsingEncoding:NSUTF8StringEncoding], YES);
  XCTAssertNotNil(params);
  nw_protocol_stack_t protocol_stack = nw_parameters_copy_default_protocol_stack(params);
  nw_protocol_options_t options = nw_protocol_stack_copy_transport_protocol(protocol_stack);
  sec_protocol_options_t sec_protocol_options = nw_tls_copy_sec_protocol_options(options);
  XCTAssertNotNil(sec_protocol_options);
  XCTAssertTrue(nw_parameters_get_include_peer_to_peer(params));
}

@end
