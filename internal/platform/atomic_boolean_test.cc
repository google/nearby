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

#include "internal/platform/atomic_boolean.h"

#include "gtest/gtest.h"

namespace nearby {
namespace {

TEST(AtomicBooleanTest, SetReturnsPrevoiusValue) {
  AtomicBoolean value(false);
  EXPECT_FALSE(value.Set(true));
  EXPECT_TRUE(value.Set(true));
}

TEST(AtomicBooleanTest, GetReturnsWhatWasSet) {
  AtomicBoolean value(false);
  EXPECT_FALSE(value.Set(true));
  EXPECT_TRUE(value.Get());
}

TEST(AtomicBooleanTest, ImplicitGetTrueValue) {
  AtomicBoolean value(true);

  EXPECT_TRUE(value);
}

TEST(AtomicBooleanTest, ImplicitGetFalseValue) {
  AtomicBoolean value(false);

  EXPECT_FALSE(value);
}
}  // namespace
}  // namespace nearby
