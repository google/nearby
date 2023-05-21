// Copyright 2021 Google LLC
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

#include "internal/platform/implementation/linux/device_info.h"

// For Linux device specific stuff
#include <unistd.h>
#include <pwd.h>

#include <array>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/base/bluetooth_address.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

std::optional<std::u16string> DeviceInfo::GetOsDeviceName() const {
  // As I know of, there is no way to fully determine the length of the Hostname (device nickname)

  // https://stackoverflow.com/a/18851841 Max Host name length is limited to 64 bytes
  char *device_name = new char[HOST_NAME_MAX];

  // Linux hostnames are limited to UTF-8, 8-bit wide characters, so we need to convert
  // that to the return type for a UTF-16 string, 16-bits wide.
  if (gethostname(device_name, HOST_NAME_MAX) == 0) {
    std::string name(device_name);
    delete[] device_name;
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().from_bytes(name);
  }

  NEARBY_LOGS(ERROR) << ": Failed to get device name, error: "
                     << strerror(errno);
  delete[] device_name;
  return std::nullopt;
}

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
  // While there is no *official* way to detect if a Linux system is a laptop or not, we can try to
  // find its chassis type and make a very educated guess as to what the user is using.
  // See https://superuser.com/a/1107191
  std::fstream chassis_type("/sys/class/dmi/id/chassis_type", std::ios::binary | std::ios::in);

  char chartype = 0;

  chassis_type.get(chartype);

  // The code is stored as text in the file, in order to get a correct
  // representation in decimal, we can subtract 48 from it.
  int type = chartype - 48;

  switch (type) {
    case 3:
    case 4:
      // Type 3 and 4 are both types of desktops
      return api::DeviceInfo::DeviceType::kDesktop;
    case 6:
    case 7:
      // Type 6 and 7 are both Towers
      return api::DeviceInfo::DeviceType::kDesktop;
    case 9:
    case 10:
      // Type 9 and 10 are laptop esc, laptop and notebook
      return api::DeviceInfo::DeviceType::kLaptop;
    case 11:
    case 30:
      // Type 11 is labled as "Hand Held"
      return api::DeviceInfo::DeviceType::kTablet;
    case 31:
    case 32:
      // Type 31 and 32 are Convertable / Detatchable
      return api::DeviceInfo::DeviceType::kLaptop;
    default:
      return api::DeviceInfo::DeviceType::kUnknown;
  }
}

api::DeviceInfo::OsType DeviceInfo::GetOsType() const {
  return api::DeviceInfo::OsType::kLinux;
}

std::optional<std::u16string> DeviceInfo::GetFullName() const {
  // We can use the C function getpwnam() to get information in a passwd database.
  // The users full name is optionally in the passwd->pw_gecos member of the passwd struct.

  struct passwd *full_user_data = getpwnam(getlogin());
  std::u16string u16str;

  if (full_user_data == nullptr) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error retrieving locally authenticated user.";
    return std::nullopt;
  }

  // The GECOS field is optional and used for information purposes only.
  // Usually it would contain the full name of the user.
  // See https://man7.org/linux/man-pages/man5/passwd.5.html
  u16str = *full_user_data->pw_gecos;

  if (u16str.empty()) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error retrieving full name of user. (GECOS field empty)";
    return std::nullopt;
  }

  return u16str;
}

std::optional<std::u16string> DeviceInfo::GetGivenName() const {
  std::optional<std::u16string> user_full_name = GetFullName();

  if (user_full_name == std::nullopt) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error retrieving first name of user.";
    return std::nullopt;
  }

  if (user_full_name->empty()) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Error unboxing string value for first name of user.";
    return std::nullopt;
  }

  std::u16string::size_type seperator = user_full_name->find_first_of(u" ");

  // If the Full Name doesn't contain a space, then we assume the whole thing is a first name
  if (seperator == std::u16string::npos) {
    return user_full_name.value();
  }

  return user_full_name->substr(*user_full_name->begin(), seperator);
}

std::optional<std::u16string> DeviceInfo::GetLastName() const {
  std::optional<std::u16string> user_full_name = GetFullName();

  if (user_full_name == std::nullopt) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error retrieving last name of user.";
    return std::nullopt;
  }

  if (user_full_name->empty()) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Error unboxing string value for last name of user.";
    return std::nullopt;
  }
  std::u16string::size_type seperator = user_full_name->find_first_of(u" ");

  // If the Full Name doesn't contain a space, then we assume there is no last name
  if (seperator == std::u16string::npos) {
    return std::nullopt;
  }

  return user_full_name->substr(*user_full_name->begin(), seperator);
}

std::optional<std::string> DeviceInfo::GetProfileUserName() const {

  std::string user_name = getlogin();


  if (user_name.empty()) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error retrieving account name of user."
                       << strerror(errno);
    return std::nullopt;
  }

  return user_name;
}

std::optional<std::filesystem::path> DeviceInfo::GetDownloadPath() const {
  // This assumes xdg-user-dir is installed on the target system, which is better
  // than guessing completely. This is 99.9% of the time going to be installed
  // automatically by the Distro/DE
  FILE *fp = popen("xdg-user-dir DOWNLOAD", "r");
  char *path = new char[512];
  if (fp == nullptr) {
    pclose(fp);
    delete fp;
    delete[] path;
    return std::nullopt;
  }

  while (fgets(path, sizeof(path), fp) != nullptr) {

  }
  int exit_status = WEXITSTATUS(fclose(fp));
  if (exit_status != 0) {
    delete fp;
    delete[] path;
    return std::nullopt;
  }

  delete fp;
  std::filesystem::path fpath = std::filesystem::path(path);
  delete[] path;
  return fpath;
}

std::optional<std::filesystem::path> DeviceInfo::GetLocalAppDataPath() const {
  // From lots of research I have done, it seems there is no way to get a path
  // for this cross distro wise, some distros might implement this path in
  // a different way. Because of that, this will have to be hard-coded and in
  // the future may be able to detect which distro and if the path is different
  // deal with that.
  
  std::string app_data_dir(getenv("HOME"));
  app_data_dir.append("/.local/share");

  if (std::filesystem::is_directory(app_data_dir)) {
      return app_data_dir;
  }

  return std::nullopt;
}

std::optional<std::filesystem::path> DeviceInfo::GetCommonAppDataPath() const {
  // From lots of research I have done, it seems there is no way to get a path
  // for this cross distro wise, some distros might implement this path in
  // a different way. Because of that, this will have to be hard-coded and in
  // the future may be able to detect which distro and if the path is different
  // deal with that.
  
  // Also there is no common appdata path on Linux

  return GetLocalAppDataPath();
}

std::optional<std::filesystem::path> DeviceInfo::GetTemporaryPath() const {
  return std::filesystem::temp_directory_path();
}

std::optional<std::filesystem::path> DeviceInfo::GetLogPath() const {
    return std::nullopt;
}

std::optional<std::filesystem::path> DeviceInfo::GetCrashDumpPath() const {
  // Crash dumps are handled by the system, it will generate a core (crash) dump
  // and put it in a directory.
  // E.g. "Segmentation fault (core dumped)"
  return std::nullopt;
}

bool DeviceInfo::IsScreenLocked() const {
  // TODO: Determine if it's actually possible to detect Linux screen lock cross DE, WM, WE, etc.
  return false;
}

void DeviceInfo::RegisterScreenLockedListener(absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) {
 //TODO: Figure out what this does
 // Assuming it has to do with detecting if the screen is locked, that may not be possible.
}

void DeviceInfo::UnregisterScreenLockedListener(absl::string_view listener_name) {
  //TODO: Figure out what this does too
  // Assuming it has to do with detecting if the screen is locked, that may not be possible.
}

}  // namespace linux
}  // namespace nearby
