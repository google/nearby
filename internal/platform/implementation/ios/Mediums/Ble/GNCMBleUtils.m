// Copyright 2022 Google LLC
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

#import "internal/platform/implementation/ios/Mediums/Ble/GNCMBleUtils.h"

#import "internal/platform/implementation/ios/GNCUtils.h"
#import "internal/platform/implementation/ios/Mediums/Ble/Sockets/Source/Shared/GNSSocket.h"
#import "GoogleToolboxForMac/GTMLogger.h"

NS_ASSUME_NONNULL_BEGIN

static const NSTimeInterval kBleSocketConnectionTimeout = 5.0;

NSData *GNCMServiceIdHash(NSString *serviceId) {
  return [GNCSha256String(serviceId)
      subdataWithRange:NSMakeRange(0, GNCMBleAdvertisementLengthServiceIdHash)];
}

@interface GNCMBleSocketDelegate : NSObject <GNSSocketDelegate>
@property(nonatomic) GNCMBoolHandler connectedHandler;
@end

@implementation GNCMBleSocketDelegate

+ (instancetype)delegateWithConnectedHandler:(GNCMBoolHandler)connectedHandler {
  GNCMBleSocketDelegate *connection = [[GNCMBleSocketDelegate alloc] init];
  connection.connectedHandler = connectedHandler;
  return connection;
}

#pragma mark - GNSSocketDelegate

- (void)socketDidConnect:(GNSSocket *)socket {
  _connectedHandler(YES);
}

- (void)socket:(GNSSocket *)socket didDisconnectWithError:(NSError *)error {
  _connectedHandler(NO);
}

- (void)socket:(GNSSocket *)socket didReceiveData:(NSData *)data {
  GTMLoggerError(@"Unexpected -didReceiveData: call");
}

@end

void GNCMWaitForConnection(GNSSocket *socket, GNCMBoolHandler completion) {
  // This function passes YES to the completion when the socket has successfully connected, and
  // otherwise passes NO to the completion after a timeout of several seconds.  We shouldn't retain
  // the completion after it's been called, so store it in a __block variable and nil it out once
  // the socket has connected.
  __block GNCMBoolHandler completionRef = completion;

  // The delegate listens for the socket connection callbacks.  It's retained by the block passed to
  // dispatch_after below, so it will live long enough to do its job.
  GNCMBleSocketDelegate *delegate =
      [GNCMBleSocketDelegate delegateWithConnectedHandler:^(BOOL didConnect) {
        dispatch_async(dispatch_get_main_queue(), ^{
          if (completionRef) completionRef(didConnect);
          completionRef = nil;
        });
      }];
  socket.delegate = delegate;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kBleSocketConnectionTimeout * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        (void)delegate;  // make sure it's retained until the timeout
        if (completionRef) completionRef(NO);
      });
}

NS_ASSUME_NONNULL_END
