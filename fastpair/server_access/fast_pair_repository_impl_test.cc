// Copyright 2023 Google LLC
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

#include "fastpair/server_access/fast_pair_repository_impl.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/repository/device_metadata.h"
#include "fastpair/repository/fake_fast_pair_metadata_repository.h"
#include "fastpair/server_access/fast_pair_metadata_downloader.h"
#include "fastpair/server_access/fast_pair_metadata_downloader_impl.h"

namespace nearby {
namespace fastpair {

constexpr absl::Duration kWaitTimeout = absl::Milliseconds(500);
const int64_t kDeviceId = 10148625;
const char kModelId[] = "9adb11";
const char kDeviceName[] = "Pixel Buds Pro";

class FakeFastPairMetadataRepositoryFactory;

namespace {
class FastPairRepositoryImplTest : public ::testing::Test {
 protected:
  struct Result {
    bool success;
    std::optional<proto::Device> device;
  };

  FastPairRepositoryImplTest() {}
  ~FastPairRepositoryImplTest() override = default;

  void GetObservedDataRequestSuccess(
      const proto::GetObservedDeviceResponse& response) {
    FakeFastPairMetadataRepository* repository =
         fake_repository_factory_->fake_repository();
    std::move(repository->get_observed_device_request()->callback)(response);
  }

  void OnSuccess(DeviceMetadata& device_metadata) {
    result_ = Result();
    result_->success = true;
    result_->device = device_metadata.GetDetails();
  }

  std::optional<Result> result_;
  FakeFastPairMetadataRepositoryFactory*
      fake_repository_factory_;
  std::unique_ptr<FastPairRepositoryImpl> repository_;
};

TEST_F(FastPairRepositoryImplTest, MetadataDownloadSuccess) {
  absl::Notification notification;

  auto fake_repository_factory =
      std::make_unique<FakeFastPairMetadataRepositoryFactory>();
  fake_repository_factory_ = fake_repository_factory.get();

  repository_ = std::make_unique<FastPairRepositoryImpl>(
      std::move(fake_repository_factory));

  repository_->Get()->GetDeviceMetadata(kModelId,
                                        [&](DeviceMetadata& device_metadata) {
                                          OnSuccess(device_metadata);
                                          notification.Notify();
                                        });
  ASSERT_TRUE(fake_repository_factory_->fake_repository() != nullptr);
  FakeFastPairMetadataRepository* repository =
      fake_repository_factory_->fake_repository();
  const proto::GetObservedDeviceRequest& request =
      repository->get_observed_device_request()->request;
  EXPECT_EQ(request.device_id(), kDeviceId);

  proto::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kDeviceId);
  response.mutable_device()->set_name(kDeviceName);
  GetObservedDataRequestSuccess(response);
  ASSERT_TRUE(result_);
  EXPECT_TRUE(result_->success);
  ASSERT_TRUE(result_->device);
  EXPECT_EQ(result_->device->name(), kDeviceName);
  EXPECT_EQ(result_->device->id(), kDeviceId);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
