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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_SESSION_DESCRIPTION_WRAPPER_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_SESSION_DESCRIPTION_WRAPPER_H_

#ifndef NO_WEBRTC

#include "webrtc/api/peer_connection_interface.h"

// Wrapper object around SessionDescriptionInterface*.
// This object owns the SessionDescriptionInterface* unless Release() has been
// called.
class SessionDescriptionWrapper {
 public:
  SessionDescriptionWrapper() = default;
  explicit SessionDescriptionWrapper(webrtc::SessionDescriptionInterface* sdp)
      : impl_(sdp) {}

  // Copy constructor that performs a deep copy, i.e. creates a new
  // SessionDescriptionInterface.
  SessionDescriptionWrapper(const SessionDescriptionWrapper& sdp) {
    if (sdp.IsValid()) {
      impl_ = webrtc::CreateSessionDescription(sdp.GetType(), sdp.ToString());
    }
  }

  SessionDescriptionWrapper(SessionDescriptionWrapper&&) = default;
  SessionDescriptionWrapper& operator=(SessionDescriptionWrapper&&) = default;

  // Release the ownership of the SessionDescriptionInterface*.
  webrtc::SessionDescriptionInterface* Release() { return impl_.release(); }

  // Returns a string representation of the sdp. Only call this, if IsValid() is
  // true.
  std::string ToString() const {
    std::string str;
    impl_->ToString(&str);
    return str;
  }

  // Returns the SdpType of the SessionDescriptionInterface. Only call this, if
  // IsValid() is true.
  webrtc::SdpType GetType() const { return impl_->GetType(); }

  const webrtc::SessionDescriptionInterface& GetSdp() { return *impl_; }

  // Return whether this object currently holds a SessionDescriptionInterface.
  bool IsValid() const { return impl_ != nullptr; }

 private:
  std::unique_ptr<webrtc::SessionDescriptionInterface> impl_;
};

#endif

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_SESSION_DESCRIPTION_WRAPPER_H_
