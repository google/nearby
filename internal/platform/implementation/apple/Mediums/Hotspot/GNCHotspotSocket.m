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

#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotSocket.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCNWConnection.h"

@interface GNCHotspotSocket ()

@property(nonatomic, nullable) id<GNCNWConnection> connection;

@end

@implementation GNCHotspotSocket {
}

- (instancetype)initWithConnection:(id<GNCNWConnection>)connection {
  self = [super init];
  if (self) {
    _connection = connection;
  }
  return self;
}

- (NSData *)readMaxLength:(NSUInteger)length error:(NSError **)error {
  if (!self.connection) {
    if (error) {
      *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EBADF userInfo:nil];
    }
    return nil;
  }

  if (length == 0) {
    if (error) {
      *error = nil;
    }
    return [NSData data];
  }

  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];

  __block NSData *blockResult = nil;
  __block NSError *blockError = nil;

  [self.connection
      receiveMessageWithMinLength:(uint32_t)length
                        maxLength:(uint32_t)length
                completionHandler:^(dispatch_data_t _Nullable content,
                                    nw_content_context_t _Nullable context, bool isComplete,
                                    nw_error_t _Nullable receiveError) {
                  [condition lock];
                  if (receiveError) {
                    blockError = (__bridge_transfer NSError *)nw_error_copy_cf_error(receiveError);
                  }
                  if (content) {
                    blockResult = (NSData *)content;
                  }
                  [condition signal];
                  [condition unlock];
                }];

  [condition wait];
  [condition unlock];

  if (error) {
    *error = blockError;
  }
  return blockResult;
}

- (BOOL)write:(NSData *)data error:(NSError **)error {
  if (!self.connection) {
    if (error) {
      *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EBADF userInfo:nil];
    }
    return NO;
  }

  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];

  __block BOOL blockResult = NO;
  __block NSError *blockError = nil;

  __block NSData *blockData = [data copy];
  dispatch_data_t dispatchData =
      dispatch_data_create(blockData.bytes, blockData.length, dispatch_get_main_queue(), ^{
        // One would assume that since `write` blocks execution until the send is complete, that
        // `data` would stay alive when needed and released at the end of the method. However, that
        // doesn't seem to be the case. We must keep a reference to `blockData` in this callback to
        // keep it alive until `dispatchData` is done being used.
        // See: b/280525159
        blockData = nil;
      });

  [self.connection sendData:dispatchData
                    context:NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT
                 isComplete:NO
          completionHandler:^(nw_error_t _Nullable sendError) {
            [condition lock];
            blockResult = sendError == nil;
            if (sendError) {
              blockError = (__bridge_transfer NSError *)nw_error_copy_cf_error(sendError);
            }
            [condition signal];
            [condition unlock];
          }];

  [condition wait];
  [condition unlock];

  if (error) {
    *error = blockError;
  }
  return blockResult;
}

- (void)close {
  if (!self.connection) {
    return;
  }
  [self.connection cancel];
  self.connection = nil;
}

@end
