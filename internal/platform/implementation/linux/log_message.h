#ifndef PLATFORM_IMPL_LINUX_LOG_MESSAGE_H_
#define PLATFORM_IMPL_LINUX_LOG_MESSAGE_H_

#include "absl/synchronization/mutex.h"
#include "glog/logging.h"
#include "internal/platform/implementation/linux/org_freedesktop_logcontrol_server_glue.h"
#include "internal/platform/implementation/log_message.h"
#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>

namespace nearby {
namespace linux {

// See documentation in
// cpp/platform/api/log_message.h
class LogMessage : public api::LogMessage {
public:
  LogMessage(const char *file, int line, Severity severity);
  ~LogMessage() override;

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
      : AdaptorInterfaces(system_bus, "/com/github/google/nearby"),
        severity_(api::LogMessage::LogMessage::Severity::kVerbose) {
    registerAdaptor();
  }
  ~LogControl() { unregisterAdaptor(); }

  void LogLevel(const LogMessage::Severity &severity) { severity_ = severity; }

  LogMessage::Severity GetLogLevel() { return severity_; }

protected:
  std::string LogLevel() override {
    switch (severity_) {
    case api::LogMessage::Severity::kVerbose:
      return "debug";
      break;
    case api::LogMessage::Severity::kInfo:
      return "info";
      break;
    case api::LogMessage::Severity::kWarning:
      return "warning";
      break;
    case api::LogMessage::Severity::kError:
      return "err";
    case api::LogMessage::Severity::kFatal:
      return "emerg";
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
    case kConsole:
      return "console";
    case kKernel:
      return "kmsg";
    case kJournal:
      return "journal";
    case kSyslog:
      return "syslog";
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

  std::string SyslogIdentifier() override {
    return "com.github.com.google.nearby";
  }

  void send(google::LogSeverity severity, const char *full_filename,
            const char *base_filename, int line, const struct ::tm *tm_time,
            const char *message, size_t message_len) override;

private:
  std::atomic<api::LogMessage::Severity> severity_;
  std::atomic<enum LogTarget> log_target_;
};
} // namespace linux
} // namespace nearby

#endif // PLATFORM_IMPL_LINUX_LOG_MESSAGE_H_
