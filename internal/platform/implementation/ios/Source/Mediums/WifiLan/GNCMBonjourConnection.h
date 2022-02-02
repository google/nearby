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

#import "internal/platform/implementation/ios/Source/Mediums/GNCMConnection.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * This medium connection sends and receives payloads over the NSInputStream and NSOutputStream
 * passed to it.
 *
 * @param inputStream The input stream to read from.
 * @param outputStream The output stream to write to.
 * @param queue The queue on which the GNCMConnection callbacks will be called. If nil, the main
 *              queue is used.
 */
@interface GNCMBonjourConnection : NSObject <GNCMConnection>
@property(nonatomic) GNCMConnectionHandlers *connectionHandlers;

- (instancetype)initWithInputStream:(NSInputStream *)inputStream
                       outputStream:(NSOutputStream *)outputStream
                              queue:(nullable dispatch_queue_t)queue;
@end

NS_ASSUME_NONNULL_END
