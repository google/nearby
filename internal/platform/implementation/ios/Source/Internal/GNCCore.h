// Copyright 2020 Google LLC
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

#import <Foundation/Foundation.h>

#include <memory>

#include "connections/core.h"
#include "connections/implementation/service_controller_router.h"
#include "internal/platform/payload_id.h"

NS_ASSUME_NONNULL_BEGIN

/** This class contains the C++ Core object. */
@interface GNCCore : NSObject {
 @public
  std::unique_ptr<::location::nearby::connections::Core> _core;
  std::unique_ptr<::location::nearby::connections::ServiceControllerRouter>
      _service_controller_router;
}

/**
 * These functions are the utilities to manipulate the InputFile in ImplementationPlatform for
 * sending File payload.
 *
 * Inserts the URL to the map, keyed by payloadID. The element will not be inserted if there
 * already is an element with the key in the map.
 */
- (void)insertURLToMapWithPayloadID:(::location::nearby::PayloadId)payloadId urlToSend:(NSURL *)url;

/**
 * Returns the URL with the payloadID and removes the entry from the map. Returns nil if
 * payloadID is not found.
 */
- (nullable NSURL *)extractURLWithPayloadID:(::location::nearby::PayloadId)payloadId;

- (void)clearSendingURLMaps;

@end

/** This function returns the Core singleton, wrapped in an Obj-C object for lifetime management. */
GNCCore *GNCGetCore();

NS_ASSUME_NONNULL_END
