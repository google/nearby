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

#include "fastpair/keyed_service/fast_pair_mediator.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/scanning/mock_scanner_broker.h"
#include "fastpair/server_access/mock_fast_pair_repository.h"

namespace nearby {
namespace fastpair {
namespace {

class MediatorTest : public testing::Test {
 public:
  void SetUp() override {
    scanner_broker_ = std::make_unique<MockScannerBroker>();
    mock_scanner_broker_ =
        static_cast<MockScannerBroker*>(scanner_broker_.get());

    fast_pair_repository_ = std::make_unique<MockFastPairRepository>();
    mock_fast_pair_repository_ =
        static_cast<MockFastPairRepository*>(fast_pair_repository_.get());
  }

  void TearDown() override {
    scanner_broker_.reset();
    fast_pair_repository_.reset();
    mediator_.reset();
    mock_scanner_broker_ = nullptr;
    mock_fast_pair_repository_ = nullptr;
  }

 protected:
  std::unique_ptr<ScannerBroker> scanner_broker_;
  std::unique_ptr<FastPairRepository> fast_pair_repository_;
  MockScannerBroker* mock_scanner_broker_;
  MockFastPairRepository* mock_fast_pair_repository_;
  std::unique_ptr<Mediator> mediator_;
};

TEST_F(MediatorTest, SannerBrokerCallStartScanning) {
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
    mediator_ = std::make_unique<Mediator>(
      std::move(scanner_broker_),
      std::move(fast_pair_repository_));
    mediator_->StartScanning();
}

}  // namespace

}  // namespace fastpair
}  // namespace nearby
