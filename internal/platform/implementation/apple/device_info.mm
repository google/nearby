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

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/device_info.h"

#import "internal/platform/implementation/apple/GNCDevicePaths.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"

namespace nearby {
namespace apple {

std::optional<std::string> DeviceInfo::GetOsDeviceName() const {
#if TARGET_OS_IPHONE
  NSString *name = UIDevice.currentDevice.name;
  const char *cName = (const char *)[name cStringUsingEncoding:NSUTF8StringEncoding];
  return std::string(cName);
#elif TARGET_OS_OSX
  NSString *name = NSHost.currentHost.localizedName;
  const char *cName = (const char *)[name cStringUsingEncoding:NSUTF8StringEncoding];
  return std::string(cName);
#elif TARGET_OS_VISION
  NSString *name = UIDevice.currentDevice.name;
  const char *cName = (const char *)[name cStringUsingEncoding:NSUTF8StringEncoding];
  return std::string(cName);
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
#elif TARGET_OS_VISION
  return api::DeviceInfo::DeviceType::kHeadset;
#else
  return api::DeviceInfo::DeviceType::kUnknown;
#endif
}

api::DeviceInfo::OsType DeviceInfo::GetOsType() const {
#if TARGET_OS_OSX
  return api::DeviceInfo::OsType::kMacOS;
#elif TARGET_OS_IPHONE
  return api::DeviceInfo::OsType::kIos;
#elif TARGET_OS_VISION
  return api::DeviceInfo::OsType::kVisionOS;
#else
  return api::DeviceInfo::OsType::kUnknown;
#endif
}

std::optional<FilePath> DeviceInfo::GetDownloadPath() const {
  NSFileManager *manager = [NSFileManager defaultManager];

  NSError *error = nil;
  NSURL *downloadsURL = [manager URLForDirectory:NSDownloadsDirectory
                                        inDomain:NSUserDomainMask
                               appropriateForURL:nil
                                          create:YES
                                           error:&error];
  if (!downloadsURL) {
    GNCLoggerError(@"Failed to get download path: %@", error);
    return std::nullopt;
  }

  return FilePath(absl::string_view([downloadsURL.path cString]));
}

std::optional<FilePath> DeviceInfo::GetLocalAppDataPath() const {
  return FilePath(absl::string_view([GNCLocalAppDataPath().path cString]));
}

std::optional<FilePath> DeviceInfo::GetCommonAppDataPath() const { return GetLocalAppDataPath(); }

std::optional<FilePath> DeviceInfo::GetTemporaryPath() const {
  return FilePath(absl::string_view([NSTemporaryDirectory() cString]));
}

std::optional<FilePath> DeviceInfo::GetLogPath() const {
  return FilePath(absl::string_view([GNCLogPath().path cString]));
}

std::optional<FilePath> DeviceInfo::GetCrashDumpPath() const {
  return FilePath(absl::string_view([GNCCrashDumpPath().path cString]));
}

bool DeviceInfo::IsScreenLocked() const { return false; }

void DeviceInfo::RegisterScreenLockedListener(
    absl::string_view listener_name, std::function<void(api::DeviceInfo::ScreenStatus)> callback) {}

void DeviceInfo::UnregisterScreenLockedListener(absl::string_view listener_name) {}

bool DeviceInfo::PreventSleep() { return true; }

bool DeviceInfo::AllowSleep() { return true; }

}  // namespace apple
}  // namespace nearby
