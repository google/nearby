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
#include "absl/strings/string_view.h"
#include "internal/proto/credential.pb.h"
#include "presence/power_mode.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {
namespace {

using ::nearby::internal::IdentityType;

constexpr absl::string_view kAccountName = "Google User";
constexpr bool kUseBle = true;
constexpr bool kOnlyScreenOnScan = true;
const IdentityType kIdentity = IdentityType::IDENTITY_TYPE_PRIVATE;
const ScanType kScanType = ScanType::kPresenceScan;
const PowerMode powerMode = PowerMode::kLowLatency;
constexpr absl::string_view kManagerAppId = "Google App Manager";
DataElement CreateTestDataElement() {
  return {DataElement::kTxPowerFieldType, "1"};
}
PresenceScanFilter CreateTestPresenceScanFilter() {
  return {.scan_type = kScanType,
          .extended_properties = {CreateTestDataElement()}};
}
LegacyPresenceScanFilter CreateTestLegacyPresenceScanFilter() {
  return {.scan_type = kScanType};
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
  builder.SetPowerMode(powerMode);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.power_mode, powerMode);
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
  std::vector<IdentityType> types = {kIdentity};
  builder.SetIdentityTypes(types);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.identity_types.size(), 1);
  EXPECT_EQ(sr.identity_types, types);
}

TEST(ScanRequestBuilderTest, TestAddScanFilter) {
  ScanRequestBuilder builder;
  PresenceScanFilter presenceScanFilter = CreateTestPresenceScanFilter();
  LegacyPresenceScanFilter legacyPresenceScanFilter =
      CreateTestLegacyPresenceScanFilter();
  builder.AddScanFilter(presenceScanFilter);
  builder.AddScanFilter(legacyPresenceScanFilter);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.scan_filters.size(), 2);
  EXPECT_TRUE(absl::holds_alternative<PresenceScanFilter>(sr.scan_filters[0]));
  EXPECT_NE(&absl::get<PresenceScanFilter>(sr.scan_filters[0]),
            &presenceScanFilter);
  EXPECT_EQ(absl::get<PresenceScanFilter>(sr.scan_filters[0]),
            presenceScanFilter);
  EXPECT_TRUE(
      absl::holds_alternative<LegacyPresenceScanFilter>(sr.scan_filters[1]));
  EXPECT_NE(&absl::get<LegacyPresenceScanFilter>(sr.scan_filters[1]),
            &legacyPresenceScanFilter);
  EXPECT_EQ(absl::get<LegacyPresenceScanFilter>(sr.scan_filters[1]),
            legacyPresenceScanFilter);
}

TEST(ScanRequestBuilderTest, TestSetScanFilters) {
  PresenceScanFilter presenceScanFilter = CreateTestPresenceScanFilter();
  LegacyPresenceScanFilter legacyPresenceScanFilter =
      CreateTestLegacyPresenceScanFilter();
  std::vector<absl::variant<PresenceScanFilter, LegacyPresenceScanFilter>>
      filterList = {presenceScanFilter, legacyPresenceScanFilter};
  ScanRequestBuilder builder;
  builder.SetScanFilters(filterList);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.scan_filters.size(), 2);
  EXPECT_TRUE(absl::holds_alternative<PresenceScanFilter>(sr.scan_filters[0]));
  EXPECT_NE(&absl::get<PresenceScanFilter>(sr.scan_filters[0]),
            &presenceScanFilter);
  EXPECT_EQ(absl::get<PresenceScanFilter>(sr.scan_filters[0]),
            presenceScanFilter);
  EXPECT_TRUE(
      absl::holds_alternative<LegacyPresenceScanFilter>(sr.scan_filters[1]));
  EXPECT_NE(&absl::get<LegacyPresenceScanFilter>(sr.scan_filters[1]),
            &legacyPresenceScanFilter);
  EXPECT_EQ(absl::get<LegacyPresenceScanFilter>(sr.scan_filters[1]),
            legacyPresenceScanFilter);
}

TEST(ScanRequestBuilderTest, TestNotEqualScanFilter) {
  ScanRequestBuilder builderLegacy, builderModern;
  ScanRequest legacy =
      builderLegacy.AddScanFilter(LegacyPresenceScanFilter{}).Build();
  ScanRequest modern =
      builderModern.AddScanFilter(PresenceScanFilter{}).Build();
  EXPECT_NE(legacy, modern);
}

TEST(ScanRequestBuilderTest, TestSetUseBle) {
  ScanRequestBuilder builder;
  builder.SetUseBle(kUseBle);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.use_ble, kUseBle);
}

TEST(ScanRequestBuilderTest, TestSetManagerAppId) {
  ScanRequestBuilder builder;
  builder.SetManagerAppId(kManagerAppId);
  ScanRequest sr = builder.Build();
  EXPECT_EQ(sr.manager_app_id, kManagerAppId);
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
                       .SetPowerMode(powerMode)
                       .SetOnlyScreenOnScan(kOnlyScreenOnScan)
                       .SetUseBle(kUseBle)
                       .SetManagerAppId(kManagerAppId)
                       .Build();
  EXPECT_EQ(sr.account_name, kAccountName);
  EXPECT_EQ(sr.scan_only_when_screen_on, kOnlyScreenOnScan);
  EXPECT_EQ(sr.power_mode, powerMode);
  EXPECT_EQ(sr.use_ble, kUseBle);
  EXPECT_EQ(sr.manager_app_id, kManagerAppId);
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
}  // namespace
}  // namespace presence
}  // namespace nearby
