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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowseResultImpl.h"

#import <Network/Network.h>
#import <XCTest/XCTest.h>

@interface GNCNWBrowseResultImplTest : XCTestCase
@end

@implementation GNCNWBrowseResultImplTest

- (void)testInit {
  GNCNWBrowseResultImpl *browseResult = [[GNCNWBrowseResultImpl alloc] init];
  XCTAssertNotNil(browseResult);
}

- (void)testGetBonjourServiceName {
  GNCNWBrowseResultImpl *browseResult = [[GNCNWBrowseResultImpl alloc] init];
  nw_endpoint_t endpoint =
      nw_endpoint_create_bonjour_service("MyService", "_servicetype._tcp", "local.");
  NSString *serviceName = [browseResult getBonjourServiceNameFromEndpoint:endpoint];
  XCTAssertEqualObjects(serviceName, @"MyService");
}

- (void)testGetBonjourServiceName_NonBonjour {
  GNCNWBrowseResultImpl *browseResult = [[GNCNWBrowseResultImpl alloc] init];
  nw_endpoint_t endpoint = nw_endpoint_create_host("google.com", "443");
  NSString *serviceName = [browseResult getBonjourServiceNameFromEndpoint:endpoint];
  XCTAssertNil(serviceName);
}

- (void)testApplyTXTRecord {
  GNCNWBrowseResultImpl *browseResult = [[GNCNWBrowseResultImpl alloc] init];
  nw_txt_record_t txtRecord = nw_txt_record_create_dictionary();
  NSData *value1Data = [@"value1" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *value2Data = [@"value2" dataUsingEncoding:NSUTF8StringEncoding];
  nw_txt_record_set_key(txtRecord, "key1", value1Data.bytes, value1Data.length);
  nw_txt_record_set_key(txtRecord, "key2", value2Data.bytes, value2Data.length);
  NSMutableDictionary<NSString *, NSString *> *results = [[NSMutableDictionary alloc] init];
  [browseResult applyTXTRecord:txtRecord
                         block:^bool(const char *key, nw_txt_record_find_key_t found,
                                     const uint8_t *value, size_t value_len) {
                           if (found == nw_txt_record_find_key_non_empty_value) {
                             NSString *valueString =
                                 [[NSString alloc] initWithBytes:value
                                                          length:value_len
                                                        encoding:NSUTF8StringEncoding];
                             [results setObject:valueString forKey:@(key)];
                           }
                           return YES;
                         }];
  XCTAssertEqual(results.count, 2);
  XCTAssertEqualObjects(results[@"key1"], @"value1");
  XCTAssertEqualObjects(results[@"key2"], @"value2");
}

@end
