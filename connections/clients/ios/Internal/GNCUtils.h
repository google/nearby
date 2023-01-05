// Copyright 2021 Google LLC
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

#ifdef __cplusplus

#import <Foundation/Foundation.h>

#include <string>

#import "connections/clients/ios/Public/NearbyConnections/GNCAdvertiser.h"
#import "connections/clients/ios/Public/NearbyConnections/GNCConnection.h"
#include "connections/listeners.h"

NS_ASSUME_NONNULL_BEGIN

namespace nearby {
namespace connections {

/** Converts GNCStrategy to Strategy. */
const Strategy& GNCStrategyToStrategy(GNCStrategy strategy);

}  // namespace connections
}  // namespace nearby

/** Internal-only properties of the connection result handlers class. */
@interface GNCConnectionResultHandlers ()
@property(nonatomic) GNCConnectionHandler successHandler;
@property(nonatomic) GNCConnectionFailureHandler failureHandler;
@end

NS_ASSUME_NONNULL_END

#endif
