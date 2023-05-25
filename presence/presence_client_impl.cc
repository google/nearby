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

#include "presence/presence_client_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "internal/platform/borrowable.h"
#include "internal/platform/logging.h"
#include "presence/presence_device.h"
#include "presence/presence_service.h"

namespace nearby {
namespace presence {

// static
PresenceClientImpl::Factory* PresenceClientImpl::Factory::g_test_factory_ =
    nullptr;

// static
std::unique_ptr<PresenceClient> PresenceClientImpl::Factory::Create(
    BorrowablePresenceService service) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(service);
  }
  return absl::WrapUnique(new PresenceClientImpl(service));
}

// static
void PresenceClientImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

PresenceClientImpl::Factory::~Factory() = default;

absl::StatusOr<ScanSessionId> PresenceClientImpl::StartScan(
    ScanRequest scan_request, ScanCallback callback) {
  ::nearby::Borrowed<PresenceService*> borrowed = service_.Borrow();
  if (!borrowed) {
    return absl::FailedPreconditionError(
        "Can't start scan, presence service is gone");
  }
  return (*borrowed)->StartScan(scan_request, std::move(callback));
}

void PresenceClientImpl::StopScan(ScanSessionId id) {
  ::nearby::Borrowed<PresenceService*> borrowed = service_.Borrow();
  if (borrowed) {
    (*borrowed)->StopScan(id);
  }
}

absl::StatusOr<BroadcastSessionId> PresenceClientImpl::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback callback) {
  ::nearby::Borrowed<PresenceService*> borrowed = service_.Borrow();
  if (!borrowed) {
    return absl::FailedPreconditionError(
        "Can't start broadcast, presence service is gone");
  }
  return (*borrowed)->StartBroadcast(broadcast_request, std::move(callback));
}

void PresenceClientImpl::StopBroadcast(BroadcastSessionId session_id) {
  ::nearby::Borrowed<PresenceService*> borrowed = service_.Borrow();
  if (borrowed) {
    (*borrowed)->StopBroadcast(session_id);
  } else {
    NEARBY_LOGS(VERBOSE) << "Session already finished, id: " << session_id;
  }
}

std::optional<PresenceDevice> PresenceClientImpl::GetLocalDevice() {
  ::nearby::Borrowed<PresenceService*> borrowed = service_.Borrow();
  if (borrowed) {
    const PresenceDevice* device = static_cast<const PresenceDevice*>(
        (*borrowed)->GetLocalDeviceProvider()->GetLocalDevice());
    return PresenceDevice(*device);
  }
  return std::nullopt;
}

}  // namespace presence
}  // namespace nearby
