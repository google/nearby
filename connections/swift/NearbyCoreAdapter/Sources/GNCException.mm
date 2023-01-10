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

#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCException.h"

#import <Foundation/Foundation.h>

// TODO(b/239758418): Change this to the non-internal version when available.
#include "internal/platform/exception.h"

#import "connections/swift/NearbyCoreAdapter/Sources/GNCException+Internal.h"

extern NSErrorDomain const GNCExceptionDomain = @"com.google.nearby.connections.exception";

namespace nearby {
namespace connections {

NSError *NSErrorFromCppException(Exception exception) {
  switch (exception.value) {
    case Exception::kSuccess:
      return nil;
    case Exception::kFailed:
      return [NSError errorWithDomain:GNCExceptionDomain code:GNCExceptionUnknown userInfo:nil];
    case Exception::kIo:
      return [NSError errorWithDomain:GNCExceptionDomain code:GNCExceptionIO userInfo:nil];
    case Exception::kInterrupted:
      return [NSError errorWithDomain:GNCExceptionDomain code:GNCExceptionInterrupted userInfo:nil];
    case Exception::kInvalidProtocolBuffer:
      return [NSError errorWithDomain:GNCExceptionDomain
                                 code:GNCExceptionInvalidProtocolBuffer
                             userInfo:nil];
    case Exception::kExecution:
      return [NSError errorWithDomain:GNCExceptionDomain code:GNCExceptionExecution userInfo:nil];
    case Exception::kTimeout:
      return [NSError errorWithDomain:GNCExceptionDomain code:GNCExceptionTimeout userInfo:nil];
    case Exception::kIllegalCharacters:
      return [NSError errorWithDomain:GNCExceptionDomain
                                 code:GNCExceptionIllegalCharacters
                             userInfo:nil];
  }
}

}  // namespace connections
}  // namespace nearby
