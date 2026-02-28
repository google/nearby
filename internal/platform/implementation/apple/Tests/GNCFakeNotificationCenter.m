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

#import "internal/platform/implementation/apple/Tests/GNCFakeNotificationCenter.h"

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface GNCFakeNotificationObserver : NSObject
@property(nonatomic, copy) NSNotificationName name;
@property(nonatomic, copy) void (^block)(NSNotification *);
@end

@implementation GNCFakeNotificationObserver
@end

@implementation GNCFakeNotificationCenter {
  NSMutableArray<GNCFakeNotificationObserver *> *_observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [NSMutableArray array];
  }
  return self;
}

- (id<NSObject>)addObserverForName:(NSNotificationName)name
                            object:(nullable id)obj
                             queue:(nullable NSOperationQueue *)queue
                        usingBlock:(void (^)(NSNotification *notification))block {
  GNCFakeNotificationObserver *observer = [[GNCFakeNotificationObserver alloc] init];
  observer.name = name;
  observer.block = block;
  @synchronized(self) {
    [_observers addObject:observer];
  }
  return observer;
}

- (void)removeObserver:(id)observer {
  @synchronized(self) {
    NSUInteger index = [_observers indexOfObjectIdenticalTo:observer];
    if (index != NSNotFound) {
      [_observers removeObjectAtIndex:index];
    }
  }
}

- (void)postNotificationName:(NSNotificationName)aName object:(nullable id)anObject {
  NSNotification *notification = [NSNotification notificationWithName:aName object:anObject];
  NSArray<GNCFakeNotificationObserver *> *observersCopy;
  @synchronized(self) {
    observersCopy = [_observers copy];
  }
  for (GNCFakeNotificationObserver *observer in observersCopy) {
    if ([observer.name isEqualToString:aName]) {
      observer.block(notification);
    }
  }
}

@end

NS_ASSUME_NONNULL_END
