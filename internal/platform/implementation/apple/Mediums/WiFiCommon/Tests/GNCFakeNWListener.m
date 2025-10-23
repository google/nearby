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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWListener.h"

#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

@implementation GNCFakeNWListener

@synthesize port = _port;

- (instancetype)init {
  self = [super init];
  if (self) {
    _stateForStart = nw_listener_state_ready;
  }
  return self;
}

- (void)setNewConnectionHandler:(void (^)(nw_connection_t connection))handler {
  self.capturedNewConnectionHandler = handler;
}

- (void)setQueue:(dispatch_queue_t)queue {
  self.capturedQueue = queue;
}

- (void)setStateChangedHandler:(void (^)(nw_listener_state_t state,
                                         nw_error_t _Nullable error))handler {
  self.capturedStateChangedHandler = handler;
  if (self.stateHandlerSetBlock) {
    self.stateHandlerSetBlock();
  }
}

- (void)start {
  self.startCalled = YES;
  if (self.simulateTimeout) {
    return;
  }
  // Trigger the state change handler asynchronously.
  if (self.capturedStateChangedHandler) {
    nw_listener_state_t state = self.simulatedError ? nw_listener_state_failed : self.stateForStart;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      // We cannot create a realistic nw_error_t, so pass nil.
      self.capturedStateChangedHandler(state, nil);
    });
  }
}

- (void)cancel {
  self.cancelCalled = YES;
  if (self.capturedStateChangedHandler) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      self.capturedStateChangedHandler(nw_listener_state_cancelled, nil);
    });
  }
}

- (void)setAdvertiseDescriptor:(nullable nw_advertise_descriptor_t)advertiseDescriptor {
  self.setAdvertiseDescriptorCalled = YES;
  self.capturedAdvertiseDescriptor = advertiseDescriptor;
}

- (void)simulateNewConnection:(nw_connection_t)connection {
  if (self.capturedNewConnectionHandler) {
    dispatch_async(self.capturedQueue ?: dispatch_get_main_queue(), ^{
      self.capturedNewConnectionHandler(connection);
    });
  }
}

@end

NS_ASSUME_NONNULL_END
