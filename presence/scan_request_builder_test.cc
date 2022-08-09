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

#include "presence/scan_request_builder.h"

#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "presence/power_mode.h"
#include "presence/presence_identity.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

constexpr char kAccountName[] = "Google User";
constexpr bool kUseBle = true;
constexpr bool kOnlyScreenOnScan = true;
const PresenceIdentity::IdentityType kIdentity =
    PresenceIdentity::IdentityType::kPrivate;
const ScanType kScanType = ScanType::kPresenceScan;
const PowerMode kPowerMode = PowerMode::kLowLatency;

std::vector<ScanFilter> MockFilters() {
  const LegacyPresenceScanFilter legacyFilter{{.scan_type = kScanType}};
  const PresenceScanFilter newFilter{{.scan_type = kScanType}};
  std::vector<ScanFilter> filters = {legacyFilter, newFilter};
  return filters;
}

TEST(ScanRequestBuilderTest, TestConstructor) {
  EXPECT_FALSE(std::is_trivially_constructible<ScanRequestBuilder>::value);
}

TEST(ScanRequestBuilderTest, TestSetAccountName) {
  ScanRequestBuilder builder;
  builder.SetAccountName(kAccountName);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.account_name, kAccountName);
}

TEST(ScanRequestBuilderTest, TestSetPowerMode) {
  ScanRequestBuilder builder;
  builder.SetPowerMode(kPowerMode);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.power_mode, kPowerMode);
}

TEST(ScanRequestBuilderTest, TestSetScanType) {
  ScanRequestBuilder builder;
  builder.SetScanType(kScanType);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.scan_type, kScanType);
}

TEST(ScanRequestBuilderTest, TestAddIdentityType) {
  ScanRequestBuilder builder;
  builder.AddIdentityType(kIdentity);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.identity_types.size(), 1);
  EXPECT_EQ(sr.identity_types[0], kIdentity);
}

TEST(ScanRequestBuilderTest, TestSetIdentityTypes) {
  ScanRequestBuilder builder;
  std::vector<PresenceIdentity::IdentityType> types = {kIdentity};
  builder.SetIdentityTypes(types);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.identity_types.size(), 1);
  EXPECT_EQ(sr.identity_types, types);
}

TEST(ScanRequestBuilderTest, TestAddScanFilter) {
  ScanRequestBuilder builder;
  LegacyPresenceScanFilter legacyFilter = {};
  legacyFilter.scan_type = kScanType;
  builder.AddScanFilter(legacyFilter);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.scan_filters.size(), 1);
  EXPECT_EQ(sr.scan_filters[0].scan_type, kScanType);
}

TEST(ScanRequestBuilderTest, TestSetScanFilters) {
  ScanRequestBuilder builder;
  std::vector<ScanFilter> filters = MockFilters();
  builder.SetScanFilters(filters);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.scan_filters.size(), 2);
  EXPECT_EQ(sr.scan_filters[0].scan_type, kScanType);
  EXPECT_EQ(sr.scan_filters[1].scan_type, kScanType);
}

TEST(ScanRequestBuilderTest, TestNotEqualScanFilter) {
  std::vector<ScanFilter> filters = MockFilters();
  ScanRequestBuilder builderLegacy, builderModern;
  ScanRequest legacy = builderLegacy.AddScanFilter(filters[0]).Build();
  ScanRequest modern = builderModern.AddScanFilter(filters[1]).Build();
  EXPECT_NE(legacy, modern);
}

TEST(ScanRequestBuilderTest, TestSetUseBle) {
  ScanRequestBuilder builder;
  builder.SetUseBle(kUseBle);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.use_ble, kUseBle);
}

TEST(ScanRequestBuilderTest, TestSetOnlyScreenOnScan) {
  ScanRequestBuilder builder;
  builder.SetOnlyScreenOnScan(kOnlyScreenOnScan);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.scan_only_when_screen_on, kOnlyScreenOnScan);
}

TEST(ScanRequestBuilderTest, TestChainCalls) {
  ScanRequestBuilder builder;
  ScanRequest sr = builder.SetAccountName(kAccountName)
                       .SetPowerMode(kPowerMode)
                       .SetOnlyScreenOnScan(kOnlyScreenOnScan)
                       .SetUseBle(kUseBle)
                       .Build();
  EXPECT_EQ(sr.account_name, kAccountName);
  EXPECT_EQ(sr.scan_only_when_screen_on, kOnlyScreenOnScan);
  EXPECT_EQ(sr.power_mode, kPowerMode);
  EXPECT_EQ(sr.use_ble, kUseBle);
}

TEST(ScanRequestBuilderTest, TestCopy) {
  ScanRequestBuilder builder1;
  builder1.SetOnlyScreenOnScan(kOnlyScreenOnScan).SetUseBle(kUseBle);
  ScanRequestBuilder builder2 = {builder1};
  EXPECT_EQ(builder1, builder2);
  ScanRequest s1 = builder1.Build();
  ScanRequest s2 = builder2.Build();
  EXPECT_EQ(s1, s2);
}

}  // namespace presence
}  // namespace nearby
