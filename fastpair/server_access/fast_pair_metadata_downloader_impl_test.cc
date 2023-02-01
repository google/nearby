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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "fastpair/common/fast_pair_http_result.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/repository/device_metadata.h"
#include "fastpair/repository/fake_fast_pair_metadata_repository.h"
#include "fastpair/server_access/fast_pair_metadata_downloader.h"

namespace nearby {
namespace fastpair {
namespace {

const int64_t kDeviceId = 10148625;
const char kModelId[] = "9adb11";
const char kDeviceName[] = "Pixel Buds Pro";

class FastPairMetadataDownloaderImplTest : public ::testing::Test {
 protected:
  struct Result {
    bool success;
    std::optional<proto::Device> device;
  };

  FastPairMetadataDownloaderImplTest() = default;
  ~FastPairMetadataDownloaderImplTest() override = default;

  void VerifyRequest() {
    ASSERT_TRUE(fake_repository_factory_.fake_repository() != nullptr);
    FakeFastPairMetadataRepository* repository =
        fake_repository_factory_.fake_repository();
    const proto::GetObservedDeviceRequest& request =
        repository->get_observed_device_request()->request;
    EXPECT_EQ(request.device_id(), kDeviceId);
  }

  void GetObservedDataRequestSuccess(
      const proto::GetObservedDeviceResponse& response) {
    VerifyRequest();
    FakeFastPairMetadataRepository* repository =
        fake_repository_factory_.fake_repository();
    std::move(repository->get_observed_device_request()->callback)(response);
  }

  void GetObservedDataRequestFailure(
      const proto::GetObservedDeviceResponse& response) {
    VerifyRequest();
    FakeFastPairMetadataRepository* repository =
        fake_repository_factory_.fake_repository();
    std::move(repository->get_observed_device_request()->error_callback)(
        FastPairHttpError::kHttpErrorInvalidArgument);
  }

  // The callbacks passed into NearbyShareContactDownloader ctor.
  void OnSuccess(DeviceMetadata& device_metadata) {
    const proto::Device device = device_metadata.GetDetails();
    result_ = Result();
    result_->success = true;
    result_->device = std::move(device);
  }
  void OnFailure() {
    result_ = Result();
    result_->success = false;
    result_->device.reset();
  }

  std::optional<Result> result_;
  FakeFastPairMetadataRepositoryFactory fake_repository_factory_;
  std::unique_ptr<FastPairMetadataDownloader> downloader_;
};

TEST_F(FastPairMetadataDownloaderImplTest, GetObservedDeviceDownloadSuccess) {
  downloader_ = FastPairMetadataDownloaderImpl::Factory::Create(
      kModelId, &fake_repository_factory_,
      [&](DeviceMetadata& device_metadata) { OnSuccess(device_metadata); },
      [&]() { OnFailure(); });
  downloader_->Run();
  proto::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kDeviceId);
  response.mutable_device()->set_name(kDeviceName);
  GetObservedDataRequestSuccess(response);
  ASSERT_TRUE(result_);
  EXPECT_TRUE(result_->success);
  ASSERT_TRUE(result_->device);
  EXPECT_EQ(result_->device->name(), kDeviceName);
  EXPECT_EQ(result_->device->id(), kDeviceId);
}

TEST_F(FastPairMetadataDownloaderImplTest, GetObservedDeviceDownloadFailure) {
  downloader_ = FastPairMetadataDownloaderImpl::Factory::Create(
      kModelId, &fake_repository_factory_,
      [&](DeviceMetadata& device_metadata) { OnSuccess(device_metadata); },
      [&]() { OnFailure(); });
  downloader_->Run();
  proto::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kDeviceId);
  response.mutable_device()->set_name(kDeviceName);
  GetObservedDataRequestFailure(response);
  ASSERT_TRUE(result_);
  EXPECT_FALSE(result_->success);
  EXPECT_FALSE(result_->device);
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
