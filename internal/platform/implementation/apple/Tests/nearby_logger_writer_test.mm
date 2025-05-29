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

#import <XCTest/XCTest.h>
#import "GoogleToolboxForMac/GTMLogger.h"

#include "internal/platform/implementation/apple/nearby_logger_writer.h"

#include <string>
#include <utility>

#include "internal/platform/logging.h"

class AbslFileLogSink : public absl::LogSink {
 public:
  AbslFileLogSink() = default;
  ~AbslFileLogSink() override = default;
  void Send(const absl::LogEntry& entry) override {
    logMessage_ = std::string(entry.text_message());
    severity_ = entry.log_severity();
  }

  std::string GetLogMessage() const { return logMessage_; }
  absl::LogSeverity GetSeverity() const { return severity_; }

 private:
  std::string logMessage_;
  absl::LogSeverity severity_ = absl::LogSeverity::kInfo;
};

@interface NearbyLoggerWriterTest : XCTestCase
@end

@implementation NearbyLoggerWriterTest {
  std::unique_ptr<AbslFileLogSink> _logSink;
}

- (void)setUp {
  [super setUp];
  _logSink = std::make_unique<AbslFileLogSink>();
  absl::SetGlobalVLogLevel(1);
  absl::AddLogSink(_logSink.get());
  nearby::apple::EnableNearbyLoggerWriter();
}

- (void)tearDown {
  absl::RemoveLogSink(_logSink.get());
  _logSink.reset();
  [super tearDown];
}

- (void)testInfoLogMessage {
  GTMLoggerInfo(@"Hello, world!");
  XCTAssertEqual(_logSink->GetLogMessage(), "Hello, world!");
  XCTAssertEqual(_logSink->GetSeverity(), absl::LogSeverity::kInfo);
}

- (void)testErrorLogMessage {
  GTMLoggerError(@"Error!");
  XCTAssertEqual(_logSink->GetLogMessage(), "Error!");
  XCTAssertEqual(_logSink->GetSeverity(), absl::LogSeverity::kError);
}

- (void)testVerboseLogMessage {
  GTMLoggerDebug(@"Verbose!");
  XCTAssertEqual(_logSink->GetLogMessage(), "Verbose!");
  XCTAssertEqual(_logSink->GetSeverity(), absl::LogSeverity::kInfo);
}

@end
