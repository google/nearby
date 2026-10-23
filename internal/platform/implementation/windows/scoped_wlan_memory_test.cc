// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/windows/scoped_wlan_memory.h"

#include <windows.h>
#include <wlanapi.h>
#include <utility>

#include "gtest/gtest.h"

namespace nearby::windows {
namespace {

TEST(ScopedWlanMemoryTest, Empty) {
  ScopedWlanMemory<WLAN_AVAILABLE_NETWORK_LIST> wlan_list;
  EXPECT_EQ(wlan_list.get(), nullptr);
}

TEST(ScopedWlanMemoryTest, Reset) {
  ScopedWlanMemory<WLAN_INTERFACE_INFO_LIST> wlan_list;
  EXPECT_EQ(wlan_list.get(), nullptr);
  WLAN_INTERFACE_INFO_LIST* wlan_list_ptr =
      reinterpret_cast<WLAN_INTERFACE_INFO_LIST*>(
          ::WlanAllocateMemory(sizeof(WLAN_INTERFACE_INFO_LIST)));
  wlan_list.Reset(wlan_list_ptr);
  EXPECT_EQ(wlan_list.get(), wlan_list_ptr);
  wlan_list.Reset();
  EXPECT_EQ(wlan_list.get(), nullptr);
}

TEST(ScopedWlanMemoryTest, ConstructWithPointer) {
  WLAN_INTERFACE_INFO_LIST* wlan_list_ptr =
      reinterpret_cast<WLAN_INTERFACE_INFO_LIST*>(
          ::WlanAllocateMemory(sizeof(WLAN_INTERFACE_INFO_LIST)));
  ScopedWlanMemory<WLAN_INTERFACE_INFO_LIST> wlan_list(wlan_list_ptr);
  EXPECT_EQ(wlan_list.get(), wlan_list_ptr);
}

TEST(ScopedWlanMemoryTest, PointerAccess) {
  WLAN_INTERFACE_INFO_LIST* wlan_list_ptr =
      reinterpret_cast<WLAN_INTERFACE_INFO_LIST*>(
          ::WlanAllocateMemory(sizeof(WLAN_INTERFACE_INFO_LIST)));
  wlan_list_ptr->dwIndex = 9834;
  ScopedWlanMemory<WLAN_INTERFACE_INFO_LIST> wlan_list(wlan_list_ptr);
  EXPECT_EQ(wlan_list->dwIndex, 9834);
}

void TestFunc(WLAN_INTERFACE_INFO_LIST** wlan_list_ptr) {
  *wlan_list_ptr = reinterpret_cast<WLAN_INTERFACE_INFO_LIST*>(
      ::WlanAllocateMemory(sizeof(WLAN_INTERFACE_INFO_LIST)));
  (*wlan_list_ptr)->dwIndex = 9834;
}

TEST(ScopedWlanMemoryTest, OutParam) {
  ScopedWlanMemory<WLAN_INTERFACE_INFO_LIST> wlan_list;
  TestFunc(wlan_list.OutParam());
  EXPECT_EQ(wlan_list->dwIndex, 9834);
}

TEST(ScopedWlanMemoryTest, MoveAssignment) {
  WLAN_INTERFACE_INFO_LIST* wlan_list_ptr =
      reinterpret_cast<WLAN_INTERFACE_INFO_LIST*>(
          ::WlanAllocateMemory(sizeof(WLAN_INTERFACE_INFO_LIST)));
  ScopedWlanMemory<WLAN_INTERFACE_INFO_LIST> wlan_list(wlan_list_ptr);
  EXPECT_EQ(wlan_list.get(), wlan_list_ptr);
  ScopedWlanMemory<WLAN_INTERFACE_INFO_LIST> wlan_list2;

  wlan_list2 = std::move(wlan_list);

  EXPECT_EQ(wlan_list2.get(), wlan_list_ptr);
}

TEST(ScopedWlanMemoryTest, Release) {
  WLAN_INTERFACE_INFO_LIST* wlan_list_ptr =
      reinterpret_cast<WLAN_INTERFACE_INFO_LIST*>(
          ::WlanAllocateMemory(sizeof(WLAN_INTERFACE_INFO_LIST)));
  ScopedWlanMemory<WLAN_INTERFACE_INFO_LIST> wlan_list(wlan_list_ptr);
  EXPECT_EQ(wlan_list.get(), wlan_list_ptr);

  EXPECT_EQ(wlan_list.Release(), wlan_list_ptr);
  EXPECT_EQ(wlan_list.get(), nullptr);
}

}  // namespace
}  // namespace nearby::windows
