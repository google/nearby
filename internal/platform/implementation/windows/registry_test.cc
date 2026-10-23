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

#include "internal/platform/implementation/windows/registry.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/windows/mock_registry_accessor.h"

namespace nearby::platform::windows {
namespace {
using ::testing::_;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Return;

constexpr absl::string_view kClientsRegKeyPath =
    R"(SOFTWARE\Google\Update\Clients\{232066FE-FF4D-4C25-83B4-3F8747CF7E3A})";
constexpr absl::string_view kClientStateRegKeyPath =
    R"(SOFTWARE\Google\Update\ClientState\{232066FE-FF4D-4C25-83B4-3F8747CF7E3A})";
constexpr absl::string_view kClientStateMediumRegKeyPath =
    R"(SOFTWARE\Google\Update\ClientStateMedium\{232066FE-FF4D-4C25-83B4-3F8747CF7E3A})";

constexpr absl::string_view kTestProductId = "{test_product_id}";
constexpr absl::string_view kTestClientsRegKeyPath =
    R"(SOFTWARE\Google\Update\Clients\{test_product_id})";
constexpr absl::string_view kTestClientStateRegKeyPath =
    R"(SOFTWARE\Google\Update\ClientState\{test_product_id})";
constexpr absl::string_view kTestClientStateMediumRegKeyPath =
    R"(SOFTWARE\Google\Update\ClientStateMedium\{test_product_id})";

absl::string_view GetTestProductId() { return kTestProductId; }

// Enable only for local runs that have the client state key created
// TODO(mattkaes): create the entire registry key chain if it's missing
TEST(DISABLED_Registry, CreateAndRead) {
  const int test_level = 4;

  bool set_success =
      Registry::SetDword(Registry::Hive::kCurrentUser,
                         Registry::Key::kClientState, "log_level", test_level);
  ASSERT_TRUE(set_success);

  auto log_level = Registry::ReadDword(
      Registry::Hive::kCurrentUser, Registry::Key::kClientState, "log_level");
  ASSERT_TRUE(log_level.has_value());
  EXPECT_EQ(test_level, log_level.value());
}

TEST(Registry, ReadDwordNonexistentValue) {
  auto reg_value = Registry::ReadDword(
      Registry::Hive::kCurrentUser, Registry::Key::kClientState,
      "NA-bc1adb34-493f-4952-8ce6-bcf2acddfbe1");
  EXPECT_THAT(reg_value.has_value(), IsFalse());
}

TEST(Registry, BlockBadHiveReadDword) {
  auto reg_value = Registry::ReadDword(
      static_cast<Registry::Hive>(891), Registry::Key::kClientState,
      "NA-bc1adb34-493f-4952-8ce6-bcf2acddfbe1");
  EXPECT_THAT(reg_value.has_value(), IsFalse());
}

TEST(Registry, BlockBadKeyReadDword) {
  auto reg_value = Registry::ReadDword(
      Registry::Hive::kCurrentUser, static_cast<Registry::Key>(891),
      "NA-bc1adb34-493f-4952-8ce6-bcf2acddfbe1");
  EXPECT_THAT(reg_value.has_value(), IsFalse());
}

TEST(Registry, ReadStringNonexistentValue) {
  auto reg_value = Registry::ReadString(
      Registry::Hive::kCurrentUser, Registry::Key::kNearbyShare,
      "NA-bc1adb34-493f-4952-8ce6-bcf2acddfbe1");
  EXPECT_THAT(reg_value.has_value(), IsFalse());
}

TEST(Registry, BlockBadHiveReadString) {
  auto reg_value = Registry::ReadString(
      static_cast<Registry::Hive>(891), Registry::Key::kClientState,
      "NA-bc1adb34-493f-4952-8ce6-bcf2acddfbe1");
  EXPECT_THAT(reg_value.has_value(), IsFalse());
}

TEST(Registry, BlockBadKeyReadString) {
  auto reg_value = Registry::ReadString(
      Registry::Hive::kCurrentUser, static_cast<Registry::Key>(891),
      "NA-bc1adb34-493f-4952-8ce6-bcf2acddfbe1");
  EXPECT_THAT(reg_value.has_value(), IsFalse());
}

TEST(Registry, BlockBadHiveWrite) {
  auto success = Registry::SetDword(
      static_cast<Registry::Hive>(891), Registry::Key::kClientState,
      "NA-bc1adb34-493f-4952-8ce6-bcf2acddfbe1", 891);
  EXPECT_THAT(success, IsFalse());
}

TEST(Registry, BlockBadKeyWrite) {
  auto success = Registry::SetDword(
      Registry::Hive::kCurrentUser, static_cast<Registry::Key>(891),
      "NA-bc1adb34-493f-4952-8ce6-bcf2acddfbe1", 891);
  EXPECT_THAT(success, IsFalse());
}

TEST(Registry, ReadStringSucceeds) {
  std::optional<std::string> result = Registry::ReadString(
      Registry::Hive::kSoftware, Registry::Key::kWindows, "ProductName");
  EXPECT_THAT(result.has_value(), IsTrue());
  EXPECT_THAT(result.value(), Not(IsEmpty()));
}

TEST(Registry, ReadDWORDSucceeds) {
  std::optional<unsigned> result =
      Registry::ReadDword(Registry::Hive::kSoftware, Registry::Key::kWindows,
                          "CurrentMajorVersionNumber");
  EXPECT_THAT(result.has_value(), IsTrue());
  EXPECT_THAT(result.value(), Not(Eq(0)));
}

// Enable only for local runs that have the client state key created
// This test requires you run with Admin privileges in order to write to
// HKEY_LOCAL_MACHINE.
TEST(Registry, DISABLED_ValidateKeyWrites) {
  const int test_value = 10;

  const std::pair<platform::windows::Registry::Hive,
                  platform::windows::Registry::Key>
      test_hives[] = {{
                          platform::windows::Registry::Hive::kCurrentUser,
                          platform::windows::Registry::Key::kClientState,
                      },
                      {
                          platform::windows::Registry::Hive::kCurrentUser,
                          platform::windows::Registry::Key::kClients,
                      },
                      {
                          platform::windows::Registry::Hive::kSoftware,
                          platform::windows::Registry::Key::kClients,
                      }};

  for (auto& key_set : test_hives) {
    auto success = platform::windows::Registry::SetDword(
        key_set.first, key_set.second, "test_key", test_value);
    EXPECT_THAT(success, IsTrue());
    if (!success) {
      std::cout << "failed to write to hive=" << static_cast<int>(key_set.first)
                << ", key=" << static_cast<int>(key_set.second) << std::endl;
    }
  }
}

TEST(Registry, ValidateReadClientState) {
  auto mock_registry_accessor =
      std::make_unique<NiceMock<MockRegistryAccessor>>();
  EXPECT_CALL(*mock_registry_accessor,
              ReadDWordValue(_, std::string(kClientStateRegKeyPath), _, _))
      .WillOnce(Return(ERROR_SUCCESS));
  Registry::SetRegistryAccessorForTest(mock_registry_accessor.get());
  (void)Registry::ReadDword(Registry::Hive::kCurrentUser,
                            Registry::Key::kClientState, "log_level");
}

TEST(Registry, ValidateReadClients) {
  auto mock_registry_accessor =
      std::make_unique<NiceMock<MockRegistryAccessor>>();
  EXPECT_CALL(*mock_registry_accessor,
              ReadStringValue(_, std::string(kClientsRegKeyPath), _, _))
      .WillOnce(Return(ERROR_SUCCESS));
  Registry::SetRegistryAccessorForTest(mock_registry_accessor.get());
  std::optional<std::string> result = Registry::ReadString(
      Registry::Hive::kCurrentUser, Registry::Key::kClients, "log_level");
}

TEST(Registry, ValidateReadClientStateMedium) {
  auto mock_registry_accessor =
      std::make_unique<NiceMock<MockRegistryAccessor>>();
  EXPECT_CALL(
      *mock_registry_accessor,
      ReadStringValue(_, std::string(kClientStateMediumRegKeyPath), _, _))
      .WillOnce(Return(ERROR_SUCCESS));
  Registry::SetRegistryAccessorForTest(mock_registry_accessor.get());
  std::optional<std::string> result =
      Registry::ReadString(Registry::Hive::kCurrentUser,
                           Registry::Key::kClientStateMedium, "log_level");
}

TEST(Registry, ValidateReadClientStateWithProductIdOverride) {
  auto mock_registry_accessor =
      std::make_unique<NiceMock<MockRegistryAccessor>>();
  EXPECT_CALL(*mock_registry_accessor,
              ReadDWordValue(_, std::string(kTestClientStateRegKeyPath), _, _))
      .WillOnce(Return(ERROR_SUCCESS));
  Registry::SetProductIdGetter(GetTestProductId);
  Registry::SetRegistryAccessorForTest(mock_registry_accessor.get());
  (void)Registry::ReadDword(Registry::Hive::kCurrentUser,
                            Registry::Key::kClientState, "log_level");
}

TEST(Registry, ValidateReadClientsWithProductIdOverride) {
  auto mock_registry_accessor =
      std::make_unique<NiceMock<MockRegistryAccessor>>();
  EXPECT_CALL(*mock_registry_accessor,
              ReadStringValue(_, std::string(kTestClientsRegKeyPath), _, _))
      .WillOnce(Return(ERROR_SUCCESS));
  Registry::SetProductIdGetter(GetTestProductId);
  Registry::SetRegistryAccessorForTest(mock_registry_accessor.get());
  std::optional<std::string> result = Registry::ReadString(
      Registry::Hive::kCurrentUser, Registry::Key::kClients, "log_level");
}

TEST(Registry, ValidateReadClientStateMediumWithProductIdOverride) {
  auto mock_registry_accessor =
      std::make_unique<NiceMock<MockRegistryAccessor>>();
  EXPECT_CALL(
      *mock_registry_accessor,
      ReadStringValue(_, std::string(kTestClientStateMediumRegKeyPath), _, _))
      .WillOnce(Return(ERROR_SUCCESS));
  Registry::SetProductIdGetter(GetTestProductId);
  Registry::SetRegistryAccessorForTest(mock_registry_accessor.get());
  std::optional<std::string> result =
      Registry::ReadString(Registry::Hive::kCurrentUser,
                           Registry::Key::kClientStateMedium, "log_level");
}

TEST(Registry, ValidateEnumValues) {
  Registry::SetRegistryAccessorForTest(nullptr);
  EXPECT_TRUE(Registry::SetDword(Registry::Hive::kCurrentUser,
                                 Registry::Key::kNearbyShareFlags, "12345",
                                 12345, /*create_sub_key=*/true));
  EXPECT_TRUE(Registry::SetString(Registry::Hive::kCurrentUser,
                                  Registry::Key::kNearbyShareFlags, "98786",
                                  "9876", /*create_sub_key=*/true));
  absl::flat_hash_map<std::string, std::string> string_values;
  absl::flat_hash_map<std::string, DWORD> dword_values;
  Registry::EnumValues(Registry::Hive::kCurrentUser,
                       Registry::Key::kNearbyShareFlags, string_values,
                       dword_values);
  EXPECT_THAT(string_values.size(), Eq(1));
  EXPECT_THAT(dword_values.size(), Eq(1));
  EXPECT_THAT(string_values["98786"], Eq("9876"));
  EXPECT_THAT(dword_values["12345"], Eq(12345));
  ::RegDeleteKeyA(HKEY_CURRENT_USER, "Google\\NearbyShare\\Flags");
}
}  // namespace
}  // namespace nearby::platform::windows
