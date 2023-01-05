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

#import "connections/clients/ios/Internal/GNCUtils.h"

#import "connections/clients/ios/Public/NearbyConnections/GNCAdvertiser.h"
#import "connections/clients/ios/Public/NearbyConnections/GNCConnection.h"
#include "connections/strategy.h"

NS_ASSUME_NONNULL_BEGIN

namespace nearby {
namespace connections {

const Strategy& GNCStrategyToStrategy(GNCStrategy strategy) {
  switch (strategy) {
    case GNCStrategyCluster:
      return Strategy::kP2pCluster;
    case GNCStrategyStar:
      return Strategy::kP2pStar;
    case GNCStrategyPointToPoint:
      return Strategy::kP2pPointToPoint;
  }
}

}  // namespace connections
}  // namespace nearby

@implementation GNCConnectionHandlers

- (instancetype)initWithBuilderBlock:(void (^)(GNCConnectionHandlers*))builderBlock {
  self = [super init];
  if (self) {
    builderBlock(self);
  }
  return self;
}

+ (instancetype)handlersWithBuilder:(void (^)(GNCConnectionHandlers * _Nonnull))builderBlock {
  return [[self alloc] initWithBuilderBlock:builderBlock];
}

@end

@implementation GNCConnectionResultHandlers

- (instancetype)initWithSuccessHandler:(GNCConnectionHandler)successHandler
                        failureHandler:(GNCConnectionFailureHandler)failureHandler {
  self = [super init];
  if (self) {
    _successHandler = successHandler;
    _failureHandler = failureHandler;
  }
  return self;
}

+ (instancetype)successHandler:(GNCConnectionHandler)successHandler
                failureHandler:(GNCConnectionFailureHandler)failureHandler {
  return [[self alloc] initWithSuccessHandler:successHandler failureHandler:failureHandler];
}

@end

NS_ASSUME_NONNULL_END
