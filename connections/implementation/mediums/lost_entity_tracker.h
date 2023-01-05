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

#ifndef CORE_INTERNAL_MEDIUMS_LOST_ENTITY_TRACKER_H_
#define CORE_INTERNAL_MEDIUMS_LOST_ENTITY_TRACKER_H_

#include "absl/container/flat_hash_set.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace connections {
namespace mediums {

// Tracks "lost" entities based on a manual update/compute model. Used by
// mediums that only report found devices. Lost entities are computed based off
// of whether a specific entity was rediscovered since the last call to
// ComputeLostEntities.
//
// Note: Entity must overload the < and == operators.
template <typename Entity>
class LostEntityTracker {
 public:
  using EntitySet = absl::flat_hash_set<Entity>;

  LostEntityTracker();
  ~LostEntityTracker();

  // Records the given entity as being recently found, whether or not this is
  // our first time discovering the entity.
  void RecordFoundEntity(const Entity& entity) ABSL_LOCKS_EXCLUDED(mutex_);

  // Computes and returns the set of entities considered lost since the last
  // time this method was called.
  EntitySet ComputeLostEntities() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Mutex mutex_;
  EntitySet current_entities_ ABSL_GUARDED_BY(mutex_);
  EntitySet previously_found_entities_ ABSL_GUARDED_BY(mutex_);
};

template <typename Entity>
LostEntityTracker<Entity>::LostEntityTracker()
    : current_entities_{}, previously_found_entities_{} {}

template <typename Entity>
LostEntityTracker<Entity>::~LostEntityTracker() {
  previously_found_entities_.clear();
  current_entities_.clear();
}

template <typename Entity>
void LostEntityTracker<Entity>::RecordFoundEntity(const Entity& entity) {
  MutexLock lock(&mutex_);

  current_entities_.insert(entity);
}

template <typename Entity>
typename LostEntityTracker<Entity>::EntitySet
LostEntityTracker<Entity>::ComputeLostEntities() {
  MutexLock lock(&mutex_);

  // The set of lost entities is the previously found set MINUS the currently
  // found set.
  for (const auto& item : current_entities_) {
    previously_found_entities_.erase(item);
  }
  auto lost_entities = std::move(previously_found_entities_);
  previously_found_entities_ = std::move(current_entities_);
  current_entities_ = {};

  return lost_entities;
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_LOST_ENTITY_TRACKER_H_
