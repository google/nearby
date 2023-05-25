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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_IMPL_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_IMPL_H_

#include <memory>
#include <optional>

#include "absl/status/statusor.h"
#include "internal/platform/borrowable.h"
#include "presence/broadcast_request.h"
#include "presence/data_types.h"
#include "presence/presence_client.h"
#include "presence/presence_device.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

class PresenceClientImpl : public PresenceClient{
 public:
  using BorrowablePresenceService = ::nearby::Borrowable<PresenceService*>;

  class Factory {
   public:
    static std::unique_ptr<PresenceClient> Create(
            BorrowablePresenceService service);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<PresenceClient> CreateInstance(
            BorrowablePresenceService service) = 0;

   private:
    static Factory* g_test_factory_;
  };

  PresenceClientImpl(const PresenceClientImpl&) = delete;
  PresenceClientImpl(PresenceClientImpl&&) = default;
  PresenceClientImpl& operator=(const PresenceClientImpl&) = delete;
  ~PresenceClientImpl() override = default;

  // PresenceClient:
  absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                          ScanCallback callback) override;
  void StopScan(ScanSessionId session_id) override;
  absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback) override;
  void StopBroadcast(BroadcastSessionId session_id) override;
  std::optional<PresenceDevice> GetLocalDevice() override;

 private:
  explicit PresenceClientImpl(BorrowablePresenceService service)
      : service_(service) {}

  BorrowablePresenceService service_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_IMPL_H_
