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
#include "location/nearby/sharing/lib/sync/sync_manager.h"

#include <optional>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "location/nearby/sharing/lib/sync/sync_binding_prefs.pb.h"
#include "location/nearby/sharing/lib/sync/sync_config_prefs.pb.h"
#include "third_party/gloop/util/time/protoutil.h"

namespace nearby::sharing {

bool SyncManager::HasSyncBindings() {
  std::optional<sync::SyncBindingPrefs> prefs =
      preference_manager_.GetSyncBindingValue();
  return prefs.has_value() && prefs->sync_bindings_size() > 0;
}

void SyncManager::SetSyncConfigBackupTime(absl::string_view binding_id,
                                          bool success, absl::Time time) {
  absl::StatusOr<google::protobuf::Timestamp> timestamp =
      util_time::EncodeGoogleApiProto(time);
  if (!timestamp.ok()) return;
  sync::SyncConfigPrefs config =
      preference_manager_.GetSyncConfigValue(binding_id)
          .value_or(sync::SyncConfigPrefs());
  if (success) {
    *config.mutable_last_successful_sync_timestamp() = *timestamp;
  } else {
    *config.mutable_last_failed_sync_timestamp() = *timestamp;
  }
  preference_manager_.SetSyncConfigValue(binding_id, config);
}

void SyncManager::AsyncInitiateSyncBinding(
    absl::AnyInvocable<void(absl::StatusOr<std::string> binding_id)>
        callback) {
  callback(absl::UnavailableError(
      "File sync bindings need the Nearby identity service, which is not "
      "available in this build."));
}

void SyncManager::AddSyncBinding(const sync::SyncBinding& binding) {
  sync::SyncBindingPrefs prefs = preference_manager_.GetSyncBindingValue()
                                     .value_or(sync::SyncBindingPrefs());
  for (sync::SyncBinding& existing : *prefs.mutable_sync_bindings()) {
    if (existing.binding_id() == binding.binding_id()) {
      existing = binding;
      preference_manager_.SetSyncBindingValue(prefs);
      return;
    }
  }
  *prefs.add_sync_bindings() = binding;
  preference_manager_.SetSyncBindingValue(prefs);
}

std::optional<sync::SyncBinding> SyncManager::GetSyncBinding(
    absl::string_view binding_id) const {
  std::optional<sync::SyncBindingPrefs> prefs =
      preference_manager_.GetSyncBindingValue();
  if (!prefs.has_value()) return std::nullopt;
  for (const sync::SyncBinding& binding : prefs->sync_bindings()) {
    if (binding.binding_id() == binding_id) return binding;
  }
  return std::nullopt;
}

absl::StatusOr<FilePath> SyncManager::UpdateSyncBindingDestinationDirectory(
    absl::string_view binding_id, const FilePath& destination_directory) {
  sync::SyncBindingPrefs prefs = preference_manager_.GetSyncBindingValue()
                                     .value_or(sync::SyncBindingPrefs());
  for (sync::SyncBinding& binding : *prefs.mutable_sync_bindings()) {
    if (binding.binding_id() == binding_id) {
      binding.set_destination_directory(destination_directory.ToString());
      preference_manager_.SetSyncBindingValue(prefs);
      return destination_directory;
    }
  }
  return absl::NotFoundError("No sync binding with that ID.");
}

}  // namespace nearby::sharing
