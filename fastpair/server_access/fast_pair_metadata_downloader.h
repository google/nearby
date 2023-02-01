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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_DOWNLOADER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_DOWNLOADER_H_

#include <functional>
#include <optional>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "fastpair/repository/device_metadata.h"

namespace nearby {
namespace fastpair {

class FastPairMetadataDownloader {
 public:
  using SuccessCallback = absl::AnyInvocable<void(DeviceMetadata&)>;
  using FailureCallback = absl::AnyInvocable<void()>;

  FastPairMetadataDownloader(absl::string_view model_id,
                             SuccessCallback success_callback,
                             FailureCallback failure_callback);
  virtual ~FastPairMetadataDownloader();

  // Starts downloading the device information
  void Run();

 protected:
  absl::string_view model_id() const { return model_id_; }
  virtual void OnRun() = 0;
  void Succeed(DeviceMetadata& device_metadata);
  void Fail();

 private:
  absl::string_view model_id_;
  SuccessCallback success_callback_;
  FailureCallback failure_callback_;
  bool was_run_ = false;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_DOWNLOADER_H_
