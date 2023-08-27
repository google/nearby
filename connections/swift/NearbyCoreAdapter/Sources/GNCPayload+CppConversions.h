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

#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCPayload.h"

#ifdef __cplusplus

namespace nearby {
namespace connections {
class Payload;
}
}  // namespace nearby

#endif

@interface GNCPayload (CppConversions)

#ifdef __cplusplus
/**
 * @note @c fromCpp should not be used to convert a @c Payload created from @c toCpp. For some
 * payload types, each conversion creates a new object holding a reference to the previous,
 * resulting in potentially endless nesting of objects.
 */
+ (nonnull GNCPayload *)fromCpp:(nearby::connections::Payload)payload;
#endif

#ifdef __cplusplus
/**
 * @note @c toCPP should not be used to convert a @c GNCPayload created from @c fromCpp. For some
 * payload types, each conversion creates a new object holding a reference to the previous,
 * resulting in potentially endless nesting of objects.
 */
- (nearby::connections::Payload)toCpp;
#endif

@end
