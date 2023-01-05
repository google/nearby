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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_FILE_PATH_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_FILE_PATH_H_

#include <string>

#include "absl/strings/string_view.h"

namespace nearby {
namespace windows {

class FilePath {
 public:
  static std::wstring GetCustomSavePath(std::wstring parent_folder,
                                        std::wstring file_name);
  static std::wstring GetDownloadPath(std::wstring parent_folder,
                                      std::wstring file_name);

 private:
  // If the file already exists we add " (x)", where x is an incrementing
  // number, starting at 1, using the next non-existing number, to the
  // file name, just before the first dot, or at the end if no dot. The
  // absolute path is returned.
  static std::wstring CreateOutputFileWithRename(std::wstring path);

  static void ReplaceInvalidCharacters(std::wstring& path);
  static void SanitizePath(std::wstring& path);
  static std::wstring MutateForbiddenPathElements(std::wstring& str);
  static std::wstring GetDownloadPathInternal(std::wstring parent_folder,
                                              std::wstring file_name);
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_FILE_PATH_H_
