#ifndef PLATFORM_V2_IMPL_IOS_LOG_MESSAGE_H_
#define PLATFORM_V2_IMPL_IOS_LOG_MESSAGE_H_

#include "base/logging.h"
#include "platform_v2/api/log_message.h"

namespace location {
namespace nearby {
namespace ios {

class LogMessage : public api::LogMessage {
 public:
  LogMessage(const char* file, int line, Severity severity);
  ~LogMessage() override;

  void Print(const char* format, ...) override;

  std::ostream& Stream() override;

 private:
  absl::LogStreamer log_streamer_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_IOS_LOG_MESSAGE_H_
