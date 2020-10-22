#ifndef PLATFORM_IMPL_G3_LOG_MESSAGE_H_
#define PLATFORM_IMPL_G3_LOG_MESSAGE_H_

#include "base/logging.h"
#include "platform/api/log_message.h"

namespace location {
namespace nearby {
namespace g3 {

// See documentation in
// https://source.corp.google.com/piper///depot/google3/platform/api/log_message.h
class LogMessage : public api::LogMessage {
 public:
  LogMessage(const char* file, int line, Severity severity);
  ~LogMessage() override;

  void Print(const char* format, ...) override;

  std::ostream& Stream() override;

 private:
  absl::LogStreamer log_streamer_;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_G3_LOG_MESSAGE_H_
