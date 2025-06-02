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

#ifndef PLATFORM_IMPL_APPLE_LOG_GNC_LOGGER_H_
#define PLATFORM_IMPL_APPLE_LOG_GNC_LOGGER_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

// Definition of the LogSeverity enum.
typedef NS_ENUM(NSInteger, GNCLogSeverity) {
  GNCLogSeverityDebug = 0,  // Debug logs are mapping to VLOG(1) in ABSL logging.
  GNCLogSeverityInfo = 1,
  GNCLogSeverityWarning = 2,
  GNCLogSeverityError = 3,
  GNCLogSeverityFatal = 4,
};

/**
 * Logs a message to Nearby Connections.
 * @param fileName The name of the file where the log message is generated.
 * @param lineNumber The line number of the log message.
 * @param severity The severity of the log message.
 * @param format The format string for the log message.
 * @param ... The arguments to be passed to the format string.
 * @note This function doesn't support private and public formatting in format string at the moment.
 */
void GNCLog(const char *fileName, int lineNumber, GNCLogSeverity severity, NSString *format,
            ...);

/// Macros to log a message to Nearby Connections.
#define GNCLoggerDebug(...) GNCLog(__FILE__, __LINE__, GNCLogSeverityDebug, ##__VA_ARGS__)
#define GNCLoggerInfo(...) GNCLog(__FILE__, __LINE__, GNCLogSeverityInfo, ##__VA_ARGS__)
#define GNCLoggerWarning(...) GNCLog(__FILE__, __LINE__, GNCLogSeverityWarning, ##__VA_ARGS__)
#define GNCLoggerError(...) GNCLog(__FILE__, __LINE__, GNCLogSeverityError, ##__VA_ARGS__)
#define GNCLoggerFatal(...) GNCLog(__FILE__, __LINE__, GNCLogSeverityFatal, ##__VA_ARGS__)

#ifdef __cplusplus
}  // extern "C"
#endif

NS_ASSUME_NONNULL_END

#endif  // PLATFORM_IMPL_APPLE_LOG_GNC_LOGGER_H_
