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

#include "core/internal/mediums/lost_entity_tracker.h"

#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

template <typename Platform, typename Entity>
LostEntityTracker<Platform, Entity>::LostEntityTracker()
    : lock_(Platform::createLock()),
      current_entities_(),
      previously_found_entities_() {}

template <typename Platform, typename Entity>
LostEntityTracker<Platform, Entity>::~LostEntityTracker() {
  previously_found_entities_.clear();
  current_entities_.clear();
}

template <typename Platform, typename Entity>
void LostEntityTracker<Platform, Entity>::recordFoundEntity(
    ConstPtr<Entity> entity) {
  Synchronized s(lock_.get());

  current_entities_.insert(entity);
}

template <typename Platform, typename Entity>
typename LostEntityTracker<Platform, Entity>::EntitySet
LostEntityTracker<Platform, Entity>::computeLostEntities() {
  Synchronized s(lock_.get());

  // The set of lost entities is the previously found set MINUS the currently
  // found set.
  for (typename EntitySet::iterator it = current_entities_.begin();
       it != current_entities_.end(); ++it) {
    previously_found_entities_.erase(*it);
  }
  EntitySet lost_entities(previously_found_entities_.begin(),
                          previously_found_entities_.end());

  // Update our previous and current sets.
  previously_found_entities_.clear();
  previously_found_entities_.insert(current_entities_.begin(),
                                    current_entities_.end());
  current_entities_.clear();

  return lost_entities;
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
