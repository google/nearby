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

#include "internal/test/fake_webrtc.h"

#include <memory>

namespace nearby {

FakeWebRtcMedium::FakeWebRtcMedium(CancellationFlag* flag)
    : WebRtcMedium(), flag_(flag) {}

FakeWebRtcMedium::~FakeWebRtcMedium() = default;

std::unique_ptr<WebRtcSignalingMessenger>
FakeWebRtcMedium::GetSignalingMessenger(
    absl::string_view self_id,
    const location::nearby::connections::LocationHint& location_hint) {
  if (cancel_during_get_signaling_messenger_) {
    flag_->Cancel();
  }

  return WebRtcMedium::GetSignalingMessenger(self_id, location_hint);
}

}  // namespace nearby
