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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_GNCNOTIFICATIONCENTER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_GNCNOTIFICATIONCENTER_H_

#import <Foundation/Foundation.h>

@protocol GNCNotificationCenter <NSObject>

// Method for registering an observer
- (id<NSObject>)addObserverForName:(NSNotificationName)name
                            object:(id)obj
                             queue:(NSOperationQueue *)queue
                        usingBlock:(void (^)(NSNotification *notification))block;

// Method for unregistering an observer
- (void)removeObserver:(id)observer;

// Method for posting a notification
- (void)postNotificationName:(NSNotificationName)aName
                      object:(nullable id)anObject;

@end

@interface NSNotificationCenter () <GNCNotificationCenter>
@end

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_GNCNOTIFICATIONCENTER_H_
