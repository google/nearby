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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_WEBRTC_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_WEBRTC_H_

#include <memory>

#include "internal/platform/webrtc.h"

namespace nearby {

class FakeWebRtcMedium : public WebRtcMedium {
 public:
  explicit FakeWebRtcMedium(CancellationFlag* flag);
  FakeWebRtcMedium(FakeWebRtcMedium&&) = delete;
  FakeWebRtcMedium& operator=(FakeWebRtcMedium&&) = delete;
  ~FakeWebRtcMedium() override;

  // WebRtcMedium:
  bool IsValid() const override { return is_valid_; }

  std::unique_ptr<WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id,
      const location::nearby::connections::LocationHint& location_hint)
      override;

  void TriggerCancellationDuringGetSignalingMessenger() {
    cancel_during_get_signaling_messenger_ = true;
  }

  void SetIsValid(bool is_valid) { is_valid_ = is_valid; }

 private:
  CancellationFlag* flag_ = nullptr;
  bool is_valid_ = true;
  bool cancel_during_get_signaling_messenger_ = false;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_WEBRTC_H_
