// Copyright 2024 Google LLC
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

#include "internal/platform/implementation/windows/string_utils.h"

#include <windows.h>

#include <string>

#include "internal/platform/logging.h"

namespace nearby::windows::string_utils {

// Converts std::string to wstring
std::wstring StringToWideString(std::string str) {
  if (str.empty()) {
    return L"";
  }

  // https://docs.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
  int output_length =
      MultiByteToWideChar(/*CodePage=*/CP_UTF8,
                          /*dwFlags=*/0,
                          /*lpMultiByteStr=*/str.c_str(),
                          /*cbMultiByte=*/static_cast<int>(str.length()),
                          /*lpWideCharStr=*/nullptr,
                          /*cchWideChar=*/0);
  if (output_length == 0) {
    return L"";
  }
  std::wstring output(output_length, L'\0');
  int result = MultiByteToWideChar(
      /*CodePage=*/CP_UTF8, /*dwFlags=*/0, /*lpMultiByteStr=*/str.c_str(),
      /*cbMultiByte=*/static_cast<int>(str.length()),
      /*lpWideCharStr=*/&output[0],
      /*cchWideChar=*/output_length);
  if (result == 0) {
    LOG(INFO) << "Error converting String to Wstring. Error code: "
              << GetLastError();
    return L"";
  }
  return output;
}

// Converts wstring to std::string
std::string WideStringToString(std::wstring wstr) {
  if (wstr.empty()) {
    return "";
  }

  std::string output;
  size_t start = 0;
  size_t index = 0;

  // Iterate over the wstring buffer, chop it into wchar chunks and convert them
  // one-by-one
  do {
    index = wstr.find(L'\0', start);
    if (index == std::wstring::npos) index = wstr.length();
    if (start <= wstr.length()) {
      // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
      int size =
          WideCharToMultiByte(/*CodePage=*/CP_UTF8,
                              /*dwFlags=*/WC_ERR_INVALID_CHARS,
                              /*lpWideCharStr=*/&wstr[start],
                              /*cchWideChar=*/static_cast<int>(index - start),
                              /*lpMultiByteStr=*/nullptr,
                              /*cbMultiByte=*/0,
                              /*lpDefaultChar=*/nullptr,
                              /*lpUsedDefaultChar=*/nullptr);
      if (size == 0) {
        return "";
      }
      std::string converted_chunk = std::string(size, '\0');
      int result = WideCharToMultiByte(
          /*CodePage=*/CP_UTF8, /*dwFlags=*/WC_ERR_INVALID_CHARS,
          /*lpWideCharStr=*/&wstr[start],
          /*cchWideChar=*/static_cast<int>(index - start),
          /*lpMultiByteStr=*/&converted_chunk[0],
          /*cbMultiByte=*/static_cast<int>(converted_chunk.size()),
          /*lpDefaultChar=*/nullptr,
          /*lpUsedDefaultChar=*/nullptr);
      if (result == 0) {
        LOG(INFO) << "Error converting Wstring to String. Error code: "
                  << GetLastError();
        return "";
      }
      output.append(converted_chunk);
      // Append '\0' to handle the case of {wstring \0 wstring \0 wstring} ->
      // {string \0 string \0 string}
      if (index < wstr.length()) {
        LOG(INFO) <<  "Appending a null byte to string";
        output.append(1, '\0');
      }
    }
    start = index + 1;
  } while (index != std::wstring::npos && start < wstr.length());

  return output;
}

}  // namespace nearby::windows::string_utils
