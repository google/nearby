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

#include <cstdint>
#include <string>

#include "internal/platform/implementation/apple/os_log_sink.h"
#include "internal/platform/logging.h"

#import "GoogleToolboxForMac/GTMLogger.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * Log message with function name and message content.
 * The log message will be used to pass data from NearbyLoggerFormatter to NearbyLoggerWriter. Due
 * to the limitation of GTMLogger, we cannot get the file name and line number.
 */
@interface NearbyLogMessage : NSObject

@property(nonatomic, readonly) uint32_t messageIndex;
@property(nonatomic, readonly) NSString *functionName;
@property(nonatomic, readonly) NSString *logMessage;

/**
 * Initializes the log message with message index, function name and message content.
 *
 * @param messageIndex The message index.
 * @param functionName The function name.
 * @param logMessage The log message.
 * @return The initialized log message.
 */
- (instancetype)initWithMessageIndex:(uint32_t)messageIndex
                        functionName:(NSString *)functionName
                          logMessage:(NSString *)logMessage;
@end

@implementation NearbyLogMessage
- (instancetype)initWithMessageIndex:(uint32_t)messageIndex
                        functionName:(NSString *)functionName
                          logMessage:(NSString *)logMessage {
  if (self = [super init]) {
    _messageIndex = messageIndex;
    _functionName = [functionName copy];
    _logMessage = [logMessage copy];
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
  NSMutableArray<NearbyLogMessage *> *_messages;
}

- (instancetype)initWithMessages:(NSMutableArray<NearbyLogMessage *> *)logMessages {
  if ((self = [super init])) {
    _messages = logMessages;
  }
  return self;
}

- (void)logMessage:(NSString *)message level:(GTMLoggerLevel)level {
  @synchronized(_messages) {
    if (_messages.count == 0) {
      return;
    }

    uint32_t messageIndex = [message intValue];
    NearbyLogMessage *message = [_messages firstObject];
    [_messages removeObjectAtIndex:0];
    if (messageIndex > message.messageIndex) {
      return;
    }

    switch (level) {
      case kGTMLoggerLevelDebug:
        VLOG(1).AtLocation([message.functionName UTF8String], 0)
            << std::string([message.logMessage UTF8String]);
        break;
      case kGTMLoggerLevelInfo:
        LOG(INFO).AtLocation([message.functionName UTF8String], 0)
            << std::string([message.logMessage UTF8String]);
        break;
      case kGTMLoggerLevelError:
        LOG(ERROR).AtLocation([message.functionName UTF8String], 0)
            << std::string([message.logMessage UTF8String]);
        break;
      case kGTMLoggerLevelAssert:
        LOG(FATAL).AtLocation([message.functionName UTF8String], 0)
            << std::string([message.logMessage UTF8String]);
        break;
      case kGTMLoggerLevelUnknown:
        LOG(INFO).AtLocation([message.functionName UTF8String], 0)
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
  uint32_t _messageIndex;
  NSMutableArray<NearbyLogMessage *> *_messages;
}
@end

#pragma mark - NearbyLoggerFormatter
@implementation NearbyLoggerFormatter

- (instancetype)initWithMessages:(NSMutableArray<NearbyLogMessage *> *)logMessages {
  if (self = [super init]) {
    _messageIndex = 0;
    _messages = logMessages;
  }
  return self;
}

- (NSString *)stringForFunc:(nullable NSString *)func
                 withFormat:(NSString *)format
                     valist:(va_list)args
                      level:(GTMLoggerLevel)level {
  @synchronized(_messages) {
    NSString *prettyNameForFunc = [self prettyNameForFunc:func];
    NSString *logMessage = [super stringForFunc:func withFormat:format valist:args level:level];
    NearbyLogMessage *message = [[NearbyLogMessage alloc] initWithMessageIndex:_messageIndex
                                                                  functionName:prettyNameForFunc
                                                                    logMessage:logMessage];
    [_messages addObject:message];
    return [NSString stringWithFormat:@"%d", (int)_messageIndex++];
  }
}

@end  // NearbyLoggerFormatter

NS_ASSUME_NONNULL_END

namespace nearby {
namespace apple {
namespace {

OsLogSink *GetOsLogSink(const std::string &subsystem) {
  static OsLogSink *sink = new OsLogSink(subsystem);
  return sink;
}

}  // namespace

void EnableNearbyLoggerWriter() {
  NSMutableArray<NearbyLogMessage *> *tupleArray = [[NSMutableArray alloc] init];
  GTMLogger.sharedLogger =
      [GTMLogger loggerWithWriter:[[NearbyLoggerWriter alloc] initWithMessages:tupleArray]
                        formatter:[[NearbyLoggerFormatter alloc] initWithMessages:tupleArray]
                           filter:nil];
}

void EnableOsLog(const std::string &subsystem) { absl::AddLogSink(GetOsLogSink(subsystem)); }

}  // namespace apple
}  // namespace nearby
