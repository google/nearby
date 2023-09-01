// Copyright 2022 Google LLC
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

#import "internal/platform/implementation/apple/device_info.h"

#import <Foundation/Foundation.h>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif

#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/device_info.h"

#import "GoogleToolboxForMac/GTMLogger.h"

namespace nearby {
namespace apple {

std::optional<std::u16string> DeviceInfo::GetOsDeviceName() const {
#if TARGET_OS_IPHONE
  NSString *name = UIDevice.currentDevice.name;
  const char16_t *cName = (const char16_t *)[name cStringUsingEncoding:NSUTF16StringEncoding];
  return std::u16string(cName);
#elif TARGET_OS_OSX
  NSString *name = NSHost.currentHost.localizedName;
  const char16_t *cName = (const char16_t *)[name cStringUsingEncoding:NSUTF16StringEncoding];
  return std::u16string(cName);
#else
  return std::nullopt;
#endif
}

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
#if TARGET_OS_IOS
  if (@available(iOS 14, *)) {
    if (NSProcessInfo.processInfo.iOSAppOnMac) {
      return api::DeviceInfo::DeviceType::kLaptop;
    }
  }
  if ([UIDevice.currentDevice.model isEqual:@"iPad"]) {
    return api::DeviceInfo::DeviceType::kTablet;
  }
  if ([UIDevice.currentDevice.model isEqual:@"iPhone"]) {
    return api::DeviceInfo::DeviceType::kPhone;
  }
  return api::DeviceInfo::DeviceType::kUnknown;
#elif TARGET_OS_OSX
  return api::DeviceInfo::DeviceType::kLaptop;
#else
  return api::DeviceInfo::DeviceType::kUnknown;
#endif
}

api::DeviceInfo::OsType DeviceInfo::GetOsType() const {
#if TARGET_OS_OSX
  return api::DeviceInfo::OsType::kMacOS;
#elif TARGET_OS_IPHONE
  return api::DeviceInfo::OsType::kIos;
#else
  return api::DeviceInfo::OsType::kUnknown;
#endif
}

std::optional<std::u16string> DeviceInfo::GetFullName() const { return std::nullopt; }
std::optional<std::u16string> DeviceInfo::GetGivenName() const { return std::nullopt; }
std::optional<std::u16string> DeviceInfo::GetLastName() const { return std::nullopt; }
std::optional<std::string> DeviceInfo::GetProfileUserName() const { return std::nullopt; }

std::optional<std::filesystem::path> DeviceInfo::GetDownloadPath() const {
  NSFileManager *manager = [NSFileManager defaultManager];

  NSError *error = nil;
  NSURL *downloadsURL = [manager URLForDirectory:NSDownloadsDirectory
                                        inDomain:NSUserDomainMask
                               appropriateForURL:nil
                                          create:YES
                                           error:&error];
  if (!downloadsURL) {
    GTMLoggerError(@"Failed to get download path: %@", error);
    return std::nullopt;
  }

  return std::filesystem::path([downloadsURL.path cString]);
}

std::optional<std::filesystem::path> DeviceInfo::GetLocalAppDataPath() const {
  NSFileManager *manager = [NSFileManager defaultManager];

  NSError *error = nil;
  NSURL *applicationSupportURL = [manager URLForDirectory:NSApplicationSupportDirectory
                                                 inDomain:NSUserDomainMask
                                        appropriateForURL:nil
                                                   create:YES
                                                    error:&error];
  if (!applicationSupportURL) {
    GTMLoggerError(@"Failed to get application support path: %@", error);
    return std::nullopt;
  }

  return std::filesystem::path([applicationSupportURL.path cString]);
}

std::optional<std::filesystem::path> DeviceInfo::GetCommonAppDataPath() const {
  return GetLocalAppDataPath();
}

std::optional<std::filesystem::path> DeviceInfo::GetTemporaryPath() const {
  return std::filesystem::path([NSTemporaryDirectory() cString]);
}

std::optional<std::filesystem::path> DeviceInfo::GetLogPath() const {
  NSFileManager *manager = [NSFileManager defaultManager];

  NSError *error = nil;
  NSURL *applicationSupportURL = [manager URLForDirectory:NSApplicationSupportDirectory
                                                 inDomain:NSUserDomainMask
                                        appropriateForURL:nil
                                                   create:YES
                                                    error:&error];
  if (!applicationSupportURL) {
    GTMLoggerError(@"Failed to get application support path: %@", error);
    return std::nullopt;
  }

  // TODO(b/276937308): This should not hard-code Nearby Share's log directory, but this matches the
  // current Windows implmementation.
  NSURL *logsURL =
      [applicationSupportURL URLByAppendingPathComponent:@"Google/Nearby/Sharing/Logs"];

  return std::filesystem::path([logsURL.path cString]);
}

std::optional<std::filesystem::path> DeviceInfo::GetCrashDumpPath() const {
  NSFileManager *manager = [NSFileManager defaultManager];

  NSError *error = nil;
  NSURL *applicationSupportURL = [manager URLForDirectory:NSApplicationSupportDirectory
                                                 inDomain:NSUserDomainMask
                                        appropriateForURL:nil
                                                   create:YES
                                                    error:&error];
  if (!applicationSupportURL) {
    GTMLoggerError(@"Failed to get application support path: %@", error);
    return std::nullopt;
  }

  // TODO(b/276937308): This should not hard-code Nearby Share's crash dump directory, but this
  // matches the current Windows implmementation.
  NSURL *crashDumpsURL =
      [applicationSupportURL URLByAppendingPathComponent:@"Google/Nearby/Sharing/CrashDumps"];

  return std::filesystem::path([crashDumpsURL.path cString]);
}

bool DeviceInfo::IsScreenLocked() const { return false; }

void DeviceInfo::RegisterScreenLockedListener(
    absl::string_view listener_name, std::function<void(api::DeviceInfo::ScreenStatus)> callback) {}

void DeviceInfo::UnregisterScreenLockedListener(absl::string_view listener_name) {}

bool DeviceInfo::PreventSleep() { return true; }

bool DeviceInfo::AllowSleep() { return true; }

}  // namespace apple
}  // namespace nearby
