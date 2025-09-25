#include <optional>
#include <string>
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/base/file_path.h"

namespace nearby
{
  namespace linux
  {
    // TODO: Add proper implementations to grab device names and types from D-bus

    std::optional<std::string> DeviceInfo::GetOsDeviceName() const
    {
      return "TestLinux" ;
    };
    api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const
    {
      return api::DeviceInfo::DeviceType::kLaptop;
    };
    std::optional<FilePath> DeviceInfo::GetDownloadPath() const
    {
      char* download_path = getenv("XDG_DOWNLOAD_DIR");
      if (download_path == nullptr)
      {
        download_path = getenv("HOME");
        if (download_path != nullptr)
        {
          std::string path = std::string(download_path) + "/Downloads";
          return FilePath(path);
        }
      }
      return FilePath(std::string(download_path));
    };
    std::optional<FilePath> DeviceInfo::GetLocalAppDataPath() const
    {
      char* dir = getenv("XDG_STATE_HOME");
      if (dir == nullptr)
      {
        return FilePath("/tmp");
      }
      return FilePath(std::string(dir)).append(FilePath("com.google.nearby")) ;
    }
    std::optional<FilePath> DeviceInfo::GetCommonAppDataPath() const
    {
      return GetLocalAppDataPath();
    };
    std::optional<FilePath> DeviceInfo::GetTemporaryPath() const
    {
      return FilePath("/tmp");
    }
    std::optional<FilePath> DeviceInfo::GetLogPath() const
    {
      return FilePath("/tmp/nearby/logs");
    };
    std::optional<FilePath> DeviceInfo::GetCrashDumpPath() const
    {
      return FilePath("/tmp/nearby/crashdump");
    }
  }
}
