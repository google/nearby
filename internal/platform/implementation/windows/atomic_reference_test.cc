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

#include "internal/platform/implementation/windows/atomic_reference.h"

#include "gtest/gtest.h"

TEST(atomic_reference, SuccessfulCreation) {
  // Arrange
  nearby::windows::AtomicUint32 atomicUint32;
  uint32_t result = UINT32_MAX;
  const uint32_t expected = 0;

  // Act
  result = atomicUint32.Get();

  // Assert
  EXPECT_EQ(result, expected);
}

TEST(atomic_reference, SuccessfulMaxSet) {
  // Arrange
  nearby::windows::AtomicUint32 atomicUint32;
  uint32_t result = 0;
  const uint32_t expected = UINT32_MAX;

  // Act
  atomicUint32.Set(UINT32_MAX);
  result = atomicUint32.Get();

  // Assert
  EXPECT_EQ(result, expected);
}

TEST(atomic_reference, SuccessfulMinSet) {
  // Arrange
  nearby::windows::AtomicUint32 atomicUint32;
  uint32_t result = UINT32_MAX;
  const uint32_t expected = 0;

  // Act
  atomicUint32.Set(0);
  result = atomicUint32.Get();

  // Assert
  EXPECT_EQ(result, expected);
}

TEST(atomic_reference, SetNegativeOneReturnsMAXUINT) {
  // Arrange
  nearby::windows::AtomicUint32 atomicUint32;
  uint32_t result = 0;
  const uint32_t expected = UINT32_MAX;

  // Act
  atomicUint32.Set(-1);  // Try Set -1, should actually store UINT32_MAX
  result = atomicUint32.Get();

  // Assert
  EXPECT_EQ(result, expected);
}
