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

#include "fastpair/server_access/fast_pair_metadata_downloader_impl.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "fastpair/repository/device_metadata.h"
#include "fastpair/repository/fast_pair_metadata_repository.h"
#include "fastpair/server_access/fast_pair_metadata_downloader.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

FastPairMetadataDownloaderImpl::Factory*
    FastPairMetadataDownloaderImpl::Factory::test_factory_ = nullptr;

std::unique_ptr<FastPairMetadataDownloader>
FastPairMetadataDownloaderImpl::Factory::Create(
    absl::string_view model_id,
    FastPairMetadataRepositoryFactory* repository_factory,
    SuccessCallback success_callback, FailureCallback failure_callback) {
  if (test_factory_) {
    return test_factory_->CreateInstance(model_id, repository_factory,
                                         std::move(success_callback),
                                         std::move(failure_callback));
  }
  return absl::WrapUnique(new FastPairMetadataDownloaderImpl(
      model_id, repository_factory, std::move(success_callback),
      std::move(failure_callback)));
}

// static
void FastPairMetadataDownloaderImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

FastPairMetadataDownloaderImpl::Factory::~Factory() = default;

FastPairMetadataDownloaderImpl::~FastPairMetadataDownloaderImpl() = default;

FastPairMetadataDownloaderImpl::FastPairMetadataDownloaderImpl(
    absl::string_view model_id,
    FastPairMetadataRepositoryFactory* repository_factory,
    SuccessCallback success_callback, FailureCallback failure_callback)
    : FastPairMetadataDownloader(model_id, std::move(success_callback),
                                 std::move(failure_callback)),
      repository_factory_(repository_factory) {}

void FastPairMetadataDownloaderImpl::OnRun() {
  NEARBY_LOGS(VERBOSE) << __func__ << " : Starting metadata downloading.";
  CallAccessServer(model_id());
}

void FastPairMetadataDownloaderImpl::CallAccessServer(
    absl::string_view model_id) {
  NEARBY_LOGS(VERBOSE) << __func__
                       << ": Making server accessing RPC call to fetch device "
                          "information with model ID: "
                       << model_id;

  proto::GetObservedDeviceRequest request;
  int64_t device_id;
  CHECK(absl::SimpleHexAtoi(model_id, &device_id));
  request.set_device_id(device_id);
  request.set_mode(proto::GetObservedDeviceRequest::MODE_RELEASE);

  repository_ = repository_factory_->CreateInstance();

  repository_->GetObservedDevice(
      request,
      [&](const proto::GetObservedDeviceResponse& response) {
        NEARBY_LOGS(INFO) << __func__ << "Name: " << response.device().name();
        NEARBY_LOGS(INFO) << __func__
                          << "Image URL: " << response.device().image_url();

        NEARBY_LOGS(INFO)
            << __func__ << "StringNotification: "
            << response.strings().initial_notification_description();
        OnAccessServerSuccess(response);
      },
      [&](FastPairHttpError error) { OnAccessServerFailure(error); });
}

void FastPairMetadataDownloaderImpl::OnAccessServerFailure(
    FastPairHttpError error) {
  NEARBY_LOGS(ERROR) << __func__
                     << ": Server accessing RPC call failed with error "
                     << error;
  Fail();
}
void FastPairMetadataDownloaderImpl::OnAccessServerSuccess(
    const proto::GetObservedDeviceResponse& response) {
  DeviceMetadata device_metadata(response);

  NEARBY_LOGS(VERBOSE) << __func__ << ": Download "
                       << device_metadata.GetDetails().name() << " succeeded.";
  Succeed(device_metadata);
}

}  // namespace fastpair
}  // namespace nearby
