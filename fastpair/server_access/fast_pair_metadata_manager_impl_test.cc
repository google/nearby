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

#include "fastpair/server_access/fast_pair_metadata_manager_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/repository/fake_fast_pair_metadata_repository.h"
#include "fastpair/server_access/fast_pair_metadata_downloader.h"
#include "fastpair/server_access/fast_pair_metadata_downloader_impl.h"
#include "fastpair/server_access/fast_pair_metadata_manager.h"

namespace nearby {
namespace fastpair {

const int64_t kDeviceId = 10148625;
const char kModelId[] = "9adb11";
const char kDeviceName[] = "Pixel Buds Pro";

class FakeFastPairMetadataRepositoryFactory;

class FakeFastPairMetadataDownloader : public FastPairMetadataDownloader {
 public:
  class Factory : public FastPairMetadataDownloaderImpl::Factory {
   public:
    Factory() = default;
    ~Factory() override = default;

    FakeFastPairMetadataDownloader* instance() { return instance_; }
    FastPairMetadataRepositoryFactory* repository_factory() const {
      return repository_factory_;
    }

   private:
    std::unique_ptr<FastPairMetadataDownloader> CreateInstance(
        std::optional<std::string> model_id,
        FastPairMetadataRepositoryFactory* repository_factory,
        SuccessCallback success_callback,
        FailureCallback failure_callback) override {
      repository_factory_ = repository_factory;
      auto instance = std::make_unique<FakeFastPairMetadataDownloader>(
          model_id, std::move(success_callback), std::move(failure_callback));
      instance_ = instance.get();
      return instance;
    }

    FakeFastPairMetadataDownloader* instance_;
    FastPairMetadataRepositoryFactory* repository_factory_;
  };

  FakeFastPairMetadataDownloader(std::optional<std::string> model_id,
                                 SuccessCallback success_callback,
                                 FailureCallback failure_callback)
      : FastPairMetadataDownloader(model_id, std::move(success_callback),
                                   std::move(failure_callback)) {}
  ~FakeFastPairMetadataDownloader() override = default;
  void OnRun() override{};
  using FastPairMetadataDownloader::Fail;
  using FastPairMetadataDownloader::model_id;
  using FastPairMetadataDownloader::Succeed;
};

namespace {

class FastPairMetadataManagerImplTest
    : public ::testing::Test,
      public FastPairMetadataManager::Observer {
 protected:
  FastPairMetadataManagerImplTest() {}
  ~FastPairMetadataManagerImplTest() override = default;

  struct MetadataDownloadedNotification {
    std::optional<std::string> model_id;
    proto::Device device;
  };

  void SetUp() override {}

  void TearDown() override {
    manager_->RemoveObserver(this);
    manager_.reset();
    FastPairMetadataDownloaderImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void OnMetadataDownlownd(const std::optional<std::string>& model_id,
                           const proto::Device& device) override {
    MetadataDownloadedNotification notification;
    notification.model_id = model_id;
    notification.device = device;
    notification_ = notification;
  }

  FakeFastPairMetadataDownloader* fake_downloader() {
    return fake_downloader_factory_.instance();
  }

  MetadataDownloadedNotification notification_;
  FakeFastPairMetadataRepositoryFactory fake_repository_factory_;
  FakeFastPairMetadataDownloader::Factory fake_downloader_factory_;
  std::unique_ptr<FastPairMetadataManager> manager_;
};

TEST_F(FastPairMetadataManagerImplTest, DownloadSuccess) {
  FastPairMetadataDownloaderImpl::Factory::SetFactoryForTesting(
      &fake_downloader_factory_);
  manager_ = FastPairMetadataManagerImpl::Factory::Create(
      kModelId, &fake_repository_factory_);
  manager_->AddObserver(this);
  proto::Device device;
  device.set_id(kDeviceId);
  device.set_name(kDeviceName);
  EXPECT_EQ(&fake_repository_factory_,
            fake_downloader_factory_.repository_factory());
  EXPECT_EQ(kModelId, fake_downloader_factory_.instance()->model_id());
  fake_downloader_factory_.instance()->Succeed(device);
  // Verify notification
  EXPECT_EQ(kModelId, notification_.model_id.value());
}

TEST_F(FastPairMetadataManagerImplTest, DownloadFailure) {
  FastPairMetadataDownloaderImpl::Factory::SetFactoryForTesting(
      &fake_downloader_factory_);
  manager_ = FastPairMetadataManagerImpl::Factory::Create(
      "", &fake_repository_factory_);
  manager_->AddObserver(this);
  EXPECT_EQ(&fake_repository_factory_,
            fake_downloader_factory_.repository_factory());
  EXPECT_EQ("", fake_downloader_factory_.instance()->model_id());
  fake_downloader_factory_.instance()->Fail();
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
