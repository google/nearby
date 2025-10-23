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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWBrowseResult.h"

#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

@implementation GNCFakeNWInterface
@end

@implementation GNCFakeNWBrowseResult

- (void)enumerateInterfaces:(nw_browse_result_t)browseResult
                 usingBlock:(nw_browse_result_enumerate_interface_t)block {
  for (GNCFakeNWInterface *interface in self.interfaces) {
    if (!block((nw_interface_t)interface)) {
      break;
    }
  }
}

- (nw_interface_type_t)interfaceGetType:(nw_interface_t)interface {
  return ((GNCFakeNWInterface *)interface).type;
}

- (nullable nw_txt_record_t)copyTXTRecordObject:(nw_browse_result_t)browseResult {
  return (nw_txt_record_t)browseResult;
}

- (void)applyTXTRecord:(nw_txt_record_t)txtRecord block:(nw_txt_record_applier_t)block {
  GNCFakeNWBrowseResult *result = (GNCFakeNWBrowseResult *)txtRecord;
  if (!result.txtRecord) {
    return;
  }
  [result.txtRecord enumerateKeysAndObjectsUsingBlock:^(NSString *key, NSString *value,
                                                        BOOL *stop) {
    const char *keyUTF8 = key.UTF8String;
    const uint8_t *valueUTF8 = (const uint8_t *)value.UTF8String;
    size_t valueLen = value.length;
    block(keyUTF8, nw_txt_record_find_key_non_empty_value, valueUTF8, valueLen);
  }];
}

- (nw_browse_result_change_t)getChangesFrom:(nw_browse_result_t)oldResult
                                         to:(nw_browse_result_t)newResult {
  return self.getChangesFromResult;
}

- (nullable nw_endpoint_t)copyEndpointFromResult:(nw_browse_result_t)result {
  if (self.endpointFromResultResult) {
    return self.endpointFromResultResult;
  }
  return nw_endpoint_create_host("fakehost", "1234");
}

- (nullable NSString *)getBonjourServiceNameFromEndpoint:(nw_endpoint_t)endpoint {
  return self.getBonjourServiceNameFromEndpointResult ?: @"FakeService";
}

@end

NS_ASSUME_NONNULL_END
