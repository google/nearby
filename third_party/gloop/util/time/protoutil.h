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
#ifndef THIRD_PARTY_GLOOP_UTIL_TIME_PROTOUTIL_H_
#define THIRD_PARTY_GLOOP_UTIL_TIME_PROTOUTIL_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "google/protobuf/timestamp.pb.h"

// Stand-in for the Google-internal google.protobuf.Timestamp conversions.
namespace util_time {

// Valid Timestamp range per google/protobuf/timestamp.proto.
inline constexpr int64_t kMinTimestampSeconds = -62135596800;
inline constexpr int64_t kMaxTimestampSeconds = 253402300799;

inline absl::StatusOr<google::protobuf::Timestamp> EncodeGoogleApiProto(
    absl::Time time) {
  const int64_t seconds = absl::ToUnixSeconds(time);
  if (seconds < kMinTimestampSeconds || seconds > kMaxTimestampSeconds) {
    return absl::InvalidArgumentError("Time is out of Timestamp range.");
  }
  const absl::Duration nanos = time - absl::FromUnixSeconds(seconds);
  google::protobuf::Timestamp timestamp;
  timestamp.set_seconds(seconds);
  timestamp.set_nanos(static_cast<int32_t>(absl::ToInt64Nanoseconds(nanos)));
  return timestamp;
}

inline absl::StatusOr<absl::Time> DecodeGoogleApiProto(
    const google::protobuf::Timestamp& timestamp) {
  if (timestamp.seconds() < kMinTimestampSeconds ||
      timestamp.seconds() > kMaxTimestampSeconds || timestamp.nanos() < 0 ||
      timestamp.nanos() > 999999999) {
    return absl::InvalidArgumentError("Timestamp is out of range.");
  }
  return absl::FromUnixSeconds(timestamp.seconds()) +
         absl::Nanoseconds(timestamp.nanos());
}

}  // namespace util_time

#endif  // THIRD_PARTY_GLOOP_UTIL_TIME_PROTOUTIL_H_
