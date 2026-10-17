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
#ifndef LOCATION_NEARBY_SHARING_LIB_SYNC_SYNC_MANAGER_H_
#define LOCATION_NEARBY_SHARING_LIB_SYNC_SYNC_MANAGER_H_

#include <optional>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "location/nearby/sharing/lib/rpc/sharing_rpc_client.h"
#include "location/nearby/sharing/lib/sync/sync_binding_prefs.pb.h"
#include "sharing/internal/api/preference_manager.h"

namespace nearby::sharing {

// Stand-in for the Google-internal file-sync binding manager. Bindings and
// per-binding sync times live in preferences (read through on every call).
// Creating a binding needs the Nearby identity service, so it is unavailable.
class SyncManager {
 public:
  SyncManager(api::IdentityRpcClient* identity_client,
              api::PreferenceManager* preference_manager)
      : preference_manager_(*preference_manager) {}

  bool HasSyncBindings();
  void SetSyncConfigBackupTime(absl::string_view binding_id, bool success,
                               absl::Time time);
  void AsyncInitiateSyncBinding(
      absl::AnyInvocable<void(absl::StatusOr<std::string> binding_id)>
          callback);
  void AddSyncBinding(const sync::SyncBinding& binding);
  std::optional<sync::SyncBinding> GetSyncBinding(
      absl::string_view binding_id) const;
  absl::StatusOr<FilePath> UpdateSyncBindingDestinationDirectory(
      absl::string_view binding_id, const FilePath& destination_directory);

 private:
  api::PreferenceManager& preference_manager_;
};

}  // namespace nearby::sharing

#endif  // LOCATION_NEARBY_SHARING_LIB_SYNC_SYNC_MANAGER_H_
