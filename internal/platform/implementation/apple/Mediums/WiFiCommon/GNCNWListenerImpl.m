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

NS_ASSUME_NONNULL_BEGIN

@implementation GNCNWListenerImpl {
  nw_listener_t _listener;
}

- (nullable instancetype)initWithParameters:(nw_parameters_t)parameters {
  self = [super init];
  if (self) {
    _listener = nw_listener_create(parameters);
    if (!_listener) {
      return nil;
    }
  }
  return self;
}

- (nullable instancetype)initWithPort:(const char *)port parameters:(nw_parameters_t)parameters {
  self = [super init];
  if (self) {
    _listener = nw_listener_create_with_port(port, parameters);
    if (!_listener) {
      return nil;
    }
  }
  return self;
}

- (void)dealloc {
  [self cancel];
}

- (uint16_t)port {
  return _listener ? nw_listener_get_port(_listener) : 0;
}

- (void)setNewConnectionHandler:(void (^)(nw_connection_t connection))handler {
  if (_listener) {
    nw_listener_set_new_connection_handler(_listener, handler);
  }
}

- (void)setQueue:(dispatch_queue_t)queue {
  if (_listener) {
    nw_listener_set_queue(_listener, queue);
  }
}

- (void)setStateChangedHandler:(void (^)(nw_listener_state_t state,
                                         nw_error_t _Nullable error))handler {
  if (_listener) {
    nw_listener_set_state_changed_handler(_listener, handler);
  }
}

- (void)start {
  if (_listener) {
    nw_listener_start(_listener);
  }
}

- (void)cancel {
  if (_listener) {
    nw_listener_cancel(_listener);
    _listener = nil;
  }
}

- (void)setAdvertiseDescriptor:(nullable nw_advertise_descriptor_t)advertiseDescriptor {
  if (_listener) {
    nw_listener_set_advertise_descriptor(_listener, advertiseDescriptor);
  }
}

@end

NS_ASSUME_NONNULL_END
