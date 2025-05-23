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

#import "internal/platform/implementation/apple/nearby_logger_writer.h"

#include <string>

#include "internal/platform/logging.h"

#import "GoogleToolboxForMac/GTMLogger.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * Log message with function name and message content.
 * The log message will be used to pass data from NearbyLoggerFormatter to NearbyLoggerWriter. Due
 * to the limitation of GTMLogger, we cannot get the file name and line number.
 */
@interface NearbyLogMessage : NSObject
@property(nonatomic, readonly) NSString *funcName;
@property(nonatomic, readonly) NSString *logMessage;

- (instancetype)initWithFuncName:(NSString *)funcName logMessage:(NSString *)logMessage;
@end

@implementation NearbyLogMessage
- (instancetype)initWithFuncName:(NSString *)funcName logMessage:(NSString *)logMessage {
  if ((self = [super init])) {
    _funcName = funcName;
    _logMessage = logMessage;
  }
  return self;
}
@end

/**
 * The customised log writer for `GTMLogWriter` is used to write the GTM log message as ABSL log
 * message.
 */
@interface NearbyLoggerWriter : NSObject <GTMLogWriter>
@end

#pragma mark - NearbyLoggerWriter

@implementation NearbyLoggerWriter {
 @private
  NSMutableArray<NearbyLogMessage *> *_messages;
}

- (instancetype)initWithMessageArray:(NSMutableArray<NearbyLogMessage *> *)logMessages {
  if ((self = [super init])) {
    _messages = logMessages;
  }
  return self;
}

- (void)logMessage:(nonnull NSString *)msg level:(GTMLoggerLevel)level {
  @synchronized(_messages) {
    if (_messages.count == 0) {
      return;
    }
    NearbyLogMessage *message = [_messages firstObject];
    [_messages removeObjectAtIndex:0];

    switch (level) {
      case kGTMLoggerLevelDebug:
        VLOG(1).AtLocation([message.funcName UTF8String], 0)
            << std::string([message.logMessage UTF8String]);
        break;
      case kGTMLoggerLevelInfo:
        LOG(INFO).AtLocation([message.funcName UTF8String], 0)
            << std::string([message.logMessage UTF8String]);
        break;
      case kGTMLoggerLevelError:
        LOG(ERROR).AtLocation([message.funcName UTF8String], 0)
            << std::string([message.logMessage UTF8String]);
        break;
      case kGTMLoggerLevelAssert:
        LOG(FATAL).AtLocation([message.funcName UTF8String], 0)
            << std::string([message.logMessage UTF8String]);
        break;
      case kGTMLoggerLevelUnknown:
        LOG(INFO).AtLocation([message.funcName UTF8String], 0)
            << std::string([message.logMessage UTF8String]);
        break;
    }
  }
}

@end  // NearbyLoggerWriter

/**
 * The customised log formatter for `GTMLogBasicFormatter`. It is used to get function name and
 * message content.
 */
@interface NearbyLoggerFormatter : GTMLogBasicFormatter {
 @private
  NSMutableArray<NearbyLogMessage *> *_messages;
}
@end

#pragma mark - NearbyLoggerFormatter
@implementation NearbyLoggerFormatter

- (instancetype)initWithMessageArray:(NSMutableArray<NearbyLogMessage *> *)logMessages {
  if ((self = [super init])) {
    _messages = logMessages;
  }
  return self;
}

- (NSString *)stringForFunc:(nullable NSString *)func
                 withFormat:(NSString *)fmt
                     valist:(va_list)args
                      level:(GTMLoggerLevel)level {
  NSString *prettyNameForFunc = [self prettyNameForFunc:func];
  NSString *logMessage = [super stringForFunc:func withFormat:fmt valist:args level:level];
  NearbyLogMessage *message = [[NearbyLogMessage alloc] initWithFuncName:prettyNameForFunc
                                                              logMessage:logMessage];
  @synchronized(_messages) {
    [_messages addObject:message];
  }
  return [NSString stringWithFormat:@"%p", prettyNameForFunc];
}

@end  // NearbyLoggerFormatter

NS_ASSUME_NONNULL_END

namespace nearby {
namespace apple {

void EnableNearbyLoggerWriter() {
  NSMutableArray<NearbyLogMessage *> *tupleArray = [[NSMutableArray alloc] init];
  GTMLogger.sharedLogger =
      [GTMLogger loggerWithWriter:[[NearbyLoggerWriter alloc] initWithMessageArray:tupleArray]
                        formatter:[[NearbyLoggerFormatter alloc] initWithMessageArray:tupleArray]
                           filter:nil];
}

}  // namespace apple
}  // namespace nearby
