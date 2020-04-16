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

#include <set>

#include "platform/api/lock.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Tracks "lost" entities based on a manual update/compute model. Used by
// mediums that only report found devices. Lost entities are computed based off
// of whether a specific entity was rediscovered since the last call to
// computeLostEntities.
//
// Note: Entity must overload the < and == operators.
template <typename Platform, typename Entity>
class LostEntityTracker {
 public:
  typedef std::set<ConstPtr<Entity> > EntitySet;

  LostEntityTracker();
  ~LostEntityTracker();

  // Records the given entity as being recently found, whether or not this is
  // our first time discovering the entity.
  void recordFoundEntity(ConstPtr<Entity> entity);

  // Computes and returns the set of entities considered lost since the last
  // time this method was called.
  EntitySet computeLostEntities();

 private:
  ScopedPtr<Ptr<Lock> > lock_;
  EntitySet current_entities_;
  EntitySet previously_found_entities_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/lost_entity_tracker.cc"

#endif  // CORE_INTERNAL_MEDIUMS_LOST_ENTITY_TRACKER_H_
