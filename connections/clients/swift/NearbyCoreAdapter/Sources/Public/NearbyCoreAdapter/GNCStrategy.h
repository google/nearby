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

#import <Foundation/Foundation.h>

/**
 * Indicates the strategy or advertisement or discovery.
 */
typedef NS_CLOSED_ENUM(NSInteger, GNCStrategy) {
  GNCStrategyCluster,       // M-to-N
  GNCStrategyStar,          // 1-to-N
  GNCStrategyPointToPoint,  // 1-to-1
} NS_SWIFT_NAME(Strategy);
