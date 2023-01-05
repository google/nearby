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

#include "internal/platform/prng.h"

#include <memory>
#include <set>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"

namespace nearby {

enum class TestMode {
  kUpperHalfOfInt64,
  kLowerHalfOfInt64,
  kInt32,
  kUint32,
};

TEST(PrngTest, NextInt32) {
  std::int32_t i = Prng().NextInt32();
  EXPECT_LE(i, std::numeric_limits<std::int32_t>::max());
  EXPECT_GE(i, std::numeric_limits<std::int32_t>::min());
}

TEST(PrngTest, NextInt32GeneratesDifferentValues) {
  Prng prng;
  std::set<int32_t> values;
  for (int i = 0; i < 100; ++i) {
    auto value = prng.NextInt32();
    EXPECT_LE(value, std::numeric_limits<std::int32_t>::max());
    EXPECT_GE(value, std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(values.find(value), values.end());
    values.insert(value);
  }
}

TEST(PrngTest, NextInt32AcrossThreads) {
  absl::Mutex mutex;
  Prng prng;
  std::set<int32_t> values;
  auto func = [&prng, &values, &mutex]() {
    absl::MutexLock lock(&mutex);
    auto value = prng.NextInt32();
    EXPECT_LE(value, std::numeric_limits<std::int32_t>::max());
    EXPECT_GE(value, std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(values.find(value), values.end());
    values.insert(value);
  };
  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.push_back(std::thread(func));
  }
  for (auto &thread : threads) {
    thread.join();
  }
  EXPECT_EQ(values.size(), 10);
}

TEST(PrngTest, NextUInt32) {
  std::uint32_t i = Prng().NextUint32();
  EXPECT_LE(i, std::numeric_limits<std::uint32_t>::max());
  EXPECT_GE(i, std::numeric_limits<std::uint32_t>::min());
}

TEST(PrngTest, NextUint32GeneratesDifferentValues) {
  Prng prng;
  std::set<uint32_t> values;
  for (int i = 0; i < 100; ++i) {
    auto value = prng.NextUint32();
    EXPECT_LE(value, std::numeric_limits<std::uint32_t>::max());
    EXPECT_GE(value, std::numeric_limits<std::uint32_t>::min());
    EXPECT_EQ(values.find(value), values.end());
    values.insert(value);
  }
}

TEST(PrngTest, NextInt64) {
  std::int64_t i = Prng().NextInt64();
  EXPECT_LE(i, std::numeric_limits<std::int64_t>::max());
  EXPECT_GE(i, std::numeric_limits<std::int64_t>::min());
}

TEST(PrngTest, NextInt64GeneratesDifferentValues) {
  Prng prng;
  std::set<int64_t> values;
  for (int i = 0; i < 100; ++i) {
    auto value = prng.NextInt64();
    EXPECT_LE(i, std::numeric_limits<std::int64_t>::max());
    EXPECT_GE(i, std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(values.find(value), values.end());
    values.insert(value);
  }
}

void ValidateRandom(TestMode mode) {
  int count_all_zeros = 0;
  int count_all_ones = 0;
  std::uint32_t i;
  Prng prng;
  for (int count = 0; count < 100; ++count) {
    switch (mode) {
      case TestMode::kUpperHalfOfInt64:
        i = static_cast<std::uint32_t>(prng.NextInt64() >> 32);
        break;
      case TestMode::kLowerHalfOfInt64:
        i = static_cast<std::uint32_t>(prng.NextInt64());
        break;
      case TestMode::kInt32:
        i = static_cast<std::uint32_t>(prng.NextInt32());
        break;
      case TestMode::kUint32:
        i = static_cast<std::uint32_t>(prng.NextUint32());
        break;
    }
    if (!i) count_all_zeros++;
    if (i == 0xFFFFFFFF) count_all_ones++;
  }
  EXPECT_LE(count_all_zeros, 1);
  EXPECT_LE(count_all_ones, 1);
}

TEST(PrngTest, ValidateUpperHalfOfInt64) {
  ValidateRandom(TestMode::kUpperHalfOfInt64);
}

TEST(PrngTest, ValidateLowerHalfOfInt64) {
  ValidateRandom(TestMode::kLowerHalfOfInt64);
}

TEST(PrngTest, ValidateInt32) { ValidateRandom(TestMode::kInt32); }

TEST(PrngTest, ValidateUint32) { ValidateRandom(TestMode::kUint32); }

}  // namespace nearby
