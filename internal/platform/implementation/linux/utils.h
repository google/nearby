#ifndef PLATFORM_IMPL_LINUX_UTILS_H_
#define PLATFORM_IMPL_LINUX_UTILS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {

std::optional<Uuid> UuidFromString(const std::string &uuid_str);
std::optional<std::string> NewUuidStr();

std::string RandSSID();
std::string RandWPAPassphrase();

}  // namespace linux
}  // namespace nearby

#endif  //  PLATFORM_IMPL_LINUX_UTILS_H_
