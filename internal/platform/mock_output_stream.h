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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_MOCK_OUTPUT_STREAM_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_MOCK_OUTPUT_STREAM_H_

#include "gmock/gmock.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/output_stream.h"

namespace nearby {

class MockOutputStream : public OutputStream {
 public:
  MOCK_METHOD(Exception, Write, (const ByteArray& data), (override));
  MOCK_METHOD(Exception, Flush, (), (override));
  MOCK_METHOD(Exception, Close, (), (override));
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_MOCK_OUTPUT_STREAM_H_
