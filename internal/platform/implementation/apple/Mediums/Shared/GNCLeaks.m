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

#import "internal/platform/implementation/apple/Mediums/Shared/GNCLeaks.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"

void GNCVerifyDealloc(id object, NSTimeInterval timeInterval) {
#if DEBUG
  __weak id weakObj = object;
  NSCAssert(weakObj != nil, @"Pointer to %@ is already nil", weakObj);
  GNCLoggerInfo(@"Verifying deallocation of %@", NSStringFromClass([weakObj class]));
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeInterval * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   NSCAssert(weakObj == nil, @"%@ not deallocated.", weakObj);
                 });
#endif
}
