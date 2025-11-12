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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnectionImpl.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

@implementation GNCNWConnectionImpl {
  nw_connection_t _connection;
}

- (instancetype)initWithNWConnection:(nw_connection_t)connection {
  self = [super init];
  if (self) {
    _connection = connection;
  }
  return self;
}

- (nw_connection_t)createConnectionWithEndpoint:(nw_endpoint_t)endpoint
                                     parameters:(nw_parameters_t)parameters {
  if (_connection) {
    nw_connection_cancel(_connection);
  }
  _connection = nw_connection_create(endpoint, parameters);
  return _connection;
}

- (void)setQueue:(dispatch_queue_t)queue {
  nw_connection_set_queue(_connection, queue);
}

- (void)setStateChangedHandler:(nw_connection_state_changed_handler_t)handler {
  nw_connection_set_state_changed_handler(_connection, handler);
}

- (void)start {
  nw_connection_start(_connection);
}

- (void)cancel {
  nw_connection_cancel(_connection);
}

- (void)receiveMessageWithMinLength:(uint32_t)minIncompleteLength
                          maxLength:(uint32_t)maxLength
                  completionHandler:(void (^)(dispatch_data_t _Nullable content,
                                              nw_content_context_t _Nullable context,
                                              bool isComplete, nw_error_t _Nullable error))handler {
  nw_connection_receive(_connection, minIncompleteLength, maxLength, handler);
}

- (void)sendData:(dispatch_data_t)content
              context:(nw_content_context_t)context
           isComplete:(BOOL)isComplete
    completionHandler:(void (^)(nw_error_t _Nullable error))handler {
  nw_connection_send(_connection, content, context, isComplete, handler);
}

- (nullable nw_connection_t)nwConnection {
  return _connection;
}

@end

NS_ASSUME_NONNULL_END