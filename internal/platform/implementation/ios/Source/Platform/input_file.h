// Copyright 2020 Google LLC
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

#ifndef PLATFORM_IMPL_IOS_INPUT_FILE_H_
#define PLATFORM_IMPL_IOS_INPUT_FILE_H_

#import <Foundation/Foundation.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/input_file.h"

namespace location {
namespace nearby {
namespace ios {

/** This InputFile subclass takes input from an NSURL. */
class InputFile : public api::InputFile {
 public:
  explicit InputFile(NSURL *nsURL);
  explicit InputFile(absl::string_view file_path, std::int64_t size);

  ~InputFile() override = default;
  InputFile(InputFile &&) = default;
  InputFile &operator=(InputFile &&) = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  std::string GetFilePath() const override;
  std::int64_t GetTotalSize() const override;
  Exception Close() override;

 private:
  NSURL *nsURL_;
  NSInputStream *nsStream_;

  std::string path_;
  size_t total_size_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_IOS_INPUT_FILE_H_
