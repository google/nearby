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

#import "connections/clients/ios/Internal/GNCCore.h"

#include <utility>

#include "absl/container/internal/common.h"
#include "connections/core.h"
#include "connections/implementation/service_controller_router.h"
#import "GoogleToolboxForMac/GTMLogger.h"

using ::nearby::connections::Core;
using ::nearby::connections::ServiceControllerRouter;

@implementation GNCCore {
}

- (instancetype)init {
  GTMLoggerInfo(@"GNCCore created");
  self = [super init];
  if (self) {
    _service_controller_router = std::make_unique<ServiceControllerRouter>();
    _core = std::make_unique<Core>(_service_controller_router.get());
  }
  return self;
}

- (void)dealloc {
  _core.reset();
  _service_controller_router.reset();
  GTMLoggerInfo(@"GNCCore deallocated");
}

@end

GNCCore *GNCGetCore() {
  static NSObject *syncSingleton;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    syncSingleton = [[NSObject alloc] init];
  });

  // The purpose of keeping a weak reference to the GNCCore object is to ensure that it will be
  // released when all external strong references are gone. I.e., when the app is no longer doing
  // any NC operations, the core will be released.
  static __weak GNCCore *core;

  // Strongly reference the GNCCore object for the duration of this function to ensure it isn't
  // prematurely deallocated by ARC after being created (which can happen in optimized builds).
  GNCCore *strongCore = core;
  @synchronized(syncSingleton) {
    if (!strongCore) {
      strongCore = [[GNCCore alloc] init];
      core = strongCore;
    }
  }
  return strongCore;
}
