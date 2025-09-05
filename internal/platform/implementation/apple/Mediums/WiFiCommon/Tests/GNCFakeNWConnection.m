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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWConnection.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

@implementation GNCFakeNWConnection

- (instancetype)init {
  return [super init];
}

- (void)receiveMessageWithMinLength:(uint32_t)minIncompleteLength
                          maxLength:(uint32_t)maxLength
                  completionHandler:(void (^)(dispatch_data_t _Nullable content,
                                              nw_content_context_t _Nullable context,
                                              bool isComplete, nw_error_t _Nullable error))handler {
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    if (self.simulateReceiveFailure) {
      // We cannot create a realistic nw_error_t, so use nil content to signal failure.
      handler(nil, nil, YES, nil);
    } else {
      handler(self.dataToReceive, nil, YES, nil);
    }
  });
}

- (void)sendData:(dispatch_data_t)content
              context:(nw_content_context_t)context
           isComplete:(BOOL)isComplete
    completionHandler:(void (^)(nw_error_t _Nullable error))handler {
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    if (self.simulateSendFailure) {
      // Simulate an error by timing out, as creating a realistic nw_error_t is not possible.
    } else {
      handler(nil);
    }
  });
}

- (void)cancel {
  self.cancelCalled = YES;
}

- (nullable nw_connection_t)nwConnection {
  return nil;  // Fake doesn't wrap a real connection
}

@end

NS_ASSUME_NONNULL_END
