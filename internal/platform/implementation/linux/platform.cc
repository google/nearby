#include <filesystem>
#include <memory>
#include <string>

#include "internal/platform/implementation/linux/condition_variable.h"
#include "internal/platform/implementation/linux/mutex.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/atomic_reference.h"
#include "internal/platform/implementation/count_down_latch.h"
#include "internal/platform/implementation/linux/atomic_boolean.h"
#include "internal/platform/implementation/linux/atomic_uint32.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "log_message.h"

namespace nearby {
namespace api {
std::string ImplementationPlatform::GetCustomSavePath(const std::string &parent_folder, const std::string & file_name) {
  auto fs = std::filesystem::path(parent_folder);
  return fs / file_name;
}

std::string ImplementationPlatform::GetDownloadPath(const std::string& parent_folder, const std::string &file_name) {
  auto downloads = std::filesystem::path(getenv("XDG_DOWNLOAD_DIR"));
  
  return downloads / std::filesystem::path(parent_folder).filename() / std::filesystem::path(file_name).filename();
}

std::string ImplementationPlatform::GetDownloadPath(const std::string& file_name) {
  auto downloads = std::filesystem::path(getenv("XDG_DOWNLOAD_DIR"));
  return downloads / std::filesystem::path(file_name).filename();
}

std::string ImplementationPlatform::GetAppDataPath(const std::string &file_name) {
  auto state = std::filesystem::path(getenv("XDG_STATE_HOME"));
  return state / std::filesystem::path(file_name).filename();
}

OSName GetCurrentOS() { return OSName::kWindows; }

std::unique_ptr<api::AtomicBoolean> CreateAtomicBoolean(bool initial_value) {
  return std::make_unique<linux::AtomicBoolean>(initial_value);
}

std::unique_ptr<api::AtomicUint32> CreateAtomicUint32(std::uint32_t value) {
  return std::make_unique<linux::AtomicUint32>(value);
}

std::unique_ptr<api::CountDownLatch> ImplementationPlatform::CreateCountDownLatch(std::int32_t count) {
  return std::make_unique<shared::CountDownLatch>(count);
}

#pragma push_macro("CreateMutex")
#undef CreateMutex
std::unique_ptr<api::Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  return std::make_unique<linux::Mutex>(mode);
}
#pragma pop_macro("CreateMutex")

std::unique_ptr<api::ConditionVariable>
ImplementationPlatform::CreateConditionVariable(api::Mutex *mutex) {
  return std::make_unique<linux::ConditionVariable>(mutex);
}

std::unique_ptr<api::LogMessage> ImplementationPlatform::CreateLogMessage(
									  const char *file, int line, LogMessage::Severity severity
    ) {
  return std::make_unique<linux::LogMessage>(file, line, severity);  
}



} // namespace api
} // namespace nearby
