// Copyright 2023 Google LLC
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

#ifndef PLATFORM_IMPL_LINUX_LOG_MESSAGE_H_
#define PLATFORM_IMPL_LINUX_LOG_MESSAGE_H_

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>

#include "glog/logging.h"
#include "internal/platform/implementation/linux/generated/dbus/logcontrol/logcontrol_server.h"
#include "internal/platform/implementation/log_message.h"

namespace nearby {
namespace linux {

// See documentation in
// cpp/platform/api/log_message.h
class LogMessage : public api::LogMessage {
 public:
  LogMessage(const char *file, int line, Severity severity);
  ~LogMessage() override{};

  void Print(const char *format, ...) override;

  std::ostream &Stream() override;

 private:
  google::LogMessage log_streamer_;
  static api::LogMessage::Severity min_log_severity_;
};

class LogControl
    : public sdbus::AdaptorInterfaces<org::freedesktop::LogControl1_adaptor>,
      public google::LogSink {
 public:
  LogControl(sdbus::IConnection &system_bus)
      : AdaptorInterfaces(system_bus, "/org/freedesktop/LogControl1"),
        severity_(api::LogMessage::LogMessage::Severity::kVerbose),
        log_target_(kConsole) {
    registerAdaptor();
  }
  ~LogControl() { unregisterAdaptor(); }

  void LogLevel(const LogMessage::Severity &severity) { severity_ = severity; }

  LogMessage::Severity GetLogLevel() { return severity_; }

 protected:
  std::string LogLevel() override {
    switch (severity_) {
      case api::LogMessage::Severity::kInfo:
        return "info";
      case api::LogMessage::Severity::kWarning:
        return "warning";
      case api::LogMessage::Severity::kError:
        return "err";
      case api::LogMessage::Severity::kFatal:
        return "emerg";
      case api::LogMessage::Severity::kVerbose:
      default:
        return "debug";
    }
  }

  void LogLevel(const std::string &value) override {
    if (value == "debug")
      severity_ = api::LogMessage::Severity::kVerbose;
    else if (value == "info")
      severity_ = api::LogMessage::Severity::kInfo;
    else if (value == "warning")
      severity_ = api::LogMessage::Severity::kWarning;
    else if (value == "err")
      severity_ = api::LogMessage::Severity::kError;
    else if (value == "crit" || value == "alert" || value == "emerg")
      severity_ = api::LogMessage::Severity::kFatal;
  }

  enum LogTarget { kConsole, kKernel, kJournal, kSyslog };

  std::string LogTarget() override {
    switch (log_target_) {
      case kKernel:
        return "kmsg";
      case kJournal:
        return "journal";
      case kSyslog:
        return "syslog";
      case kConsole:
      default:
        return "console";
    }
  }

  void LogTarget(const std::string &value) override {
    if (value == "console")
      log_target_ = kConsole;
    else if (value == "kmsg")
      log_target_ = kKernel;
    else if (value == "journal")
      log_target_ = kJournal;
    else if (value == "syslog")
      log_target_ = kSyslog;
  }

  std::string SyslogIdentifier() override { return "com.google.nearby"; }

  void send(google::LogSeverity severity, const char *full_filename,
            const char *base_filename, int line, const struct ::tm *tm_time,
            const char *message, size_t message_len) override;

 private:
  std::atomic<api::LogMessage::Severity> severity_;
  std::atomic<enum LogTarget> log_target_;
};
}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_LOG_MESSAGE_H_
