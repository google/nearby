// Copyright 2023 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCWiFiLANSocket.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

#import "GoogleToolboxForMac/GTMLogger.h"

@interface GNCWiFiLANSocket ()

@property(nonatomic, readonly) nw_connection_t connection;

@end

@implementation GNCWiFiLANSocket {
}

- (instancetype)initWithConnection:(nw_connection_t)connection {
  self = [super init];
  if (self) {
    _connection = connection;
  }
  return self;
}

- (NSData *)readMaxLength:(NSUInteger)length error:(NSError **)error {
  __strong nw_connection_t connection = self.connection;
  if (connection == nil) {
    return nil;
  }

  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];

  __block NSData *blockResult = nil;
  __block NSError *blockError = nil;

  nw_connection_receive(
      connection, length, length,
      ^(dispatch_data_t content, nw_content_context_t context, bool is_complete, nw_error_t error) {
        [condition lock];
        if (error != nil) {
          blockError = (__bridge_transfer NSError *)nw_error_copy_cf_error(error);
        }
#if __LP64__
        // This cast is only safe in a 64-bit runtime.
        blockResult = (NSData *)content;
#else
        blockResult = nil;
#endif
        [condition signal];
        [condition unlock];
      });

  [condition wait];
  [condition unlock];

  if (error != nil) {
    *error = blockError;
  }
  return blockResult;
}

- (BOOL)write:(NSData *)data error:(NSError **)error {
  __strong nw_connection_t connection = self.connection;
  if (connection == nil) {
    return NO;
  }

  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];

  __block BOOL blockResult = NO;
  __block NSError *blockError = nil;

  dispatch_data_t dispatchData =
      dispatch_data_create(data.bytes, data.length, dispatch_get_main_queue(),
                           ^{
                               // Since we block until the send is complete, `data` should stay
                               // alive when needed and released at the end of this method.
                           });

  nw_connection_send(connection, dispatchData, NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, false,
                     ^(nw_error_t error) {
                       [condition lock];
                       blockResult = error == nil;
                       if (error != nil) {
                         blockError = (__bridge_transfer NSError *)nw_error_copy_cf_error(error);
                       }
                       [condition signal];
                       [condition unlock];
                     });

  [condition wait];
  [condition unlock];

  if (error != nil) {
    *error = blockError;
  }
  return blockResult;
}

- (void)close {
  __strong nw_connection_t connection = self.connection;
  if (connection == nil) {
    return;
  }
  nw_connection_cancel(connection);
  _connection = nil;
}

@end
