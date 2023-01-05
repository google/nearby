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

#include "connections/implementation/mediums/lost_entity_tracker.h"

#include "gtest/gtest.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {

struct TestEntity {
  int id;

  template <typename H>
  friend H AbslHashValue(H h, const TestEntity& test_entity) {
    return H::combine(std::move(h), test_entity.id);
  }

  bool operator==(const TestEntity& other) const { return id == other.id; }
  bool operator<(const TestEntity& other) const { return id < other.id; }
};

TEST(LostEntityTrackerTest, NoEntitiesLost) {
  LostEntityTracker<TestEntity> lost_entity_tracker;
  TestEntity entity_1{1};
  TestEntity entity_2{2};
  TestEntity entity_3{3};

  // Discover some entities.
  lost_entity_tracker.RecordFoundEntity(entity_1);
  lost_entity_tracker.RecordFoundEntity(entity_2);
  lost_entity_tracker.RecordFoundEntity(entity_3);

  // Make sure none are lost on the first round.
  ASSERT_TRUE(lost_entity_tracker.ComputeLostEntities().empty());

  // Rediscover the same entities.
  lost_entity_tracker.RecordFoundEntity(entity_1);
  lost_entity_tracker.RecordFoundEntity(entity_2);
  lost_entity_tracker.RecordFoundEntity(entity_3);

  // Make sure we still didn't lose any entities.
  EXPECT_TRUE(lost_entity_tracker.ComputeLostEntities().empty());
}

TEST(LostEntityTrackerTest, AllEntitiesLost) {
  LostEntityTracker<TestEntity> lost_entity_tracker;
  TestEntity entity_1{1};
  TestEntity entity_2{2};
  TestEntity entity_3{3};

  // Discover some entities.
  lost_entity_tracker.RecordFoundEntity(entity_1);
  lost_entity_tracker.RecordFoundEntity(entity_2);
  lost_entity_tracker.RecordFoundEntity(entity_3);

  // Make sure none are lost on the first round.
  EXPECT_TRUE(lost_entity_tracker.ComputeLostEntities().empty());

  // Go through a round without rediscovering any entities.
  typename LostEntityTracker<TestEntity>::EntitySet lost_entities =
      lost_entity_tracker.ComputeLostEntities();
  EXPECT_TRUE(lost_entities.find(entity_1) != lost_entities.end());
  EXPECT_TRUE(lost_entities.find(entity_2) != lost_entities.end());
  EXPECT_TRUE(lost_entities.find(entity_3) != lost_entities.end());
}

TEST(LostEntityTrackerTest, SomeEntitiesLost) {
  LostEntityTracker<TestEntity> lost_entity_tracker;
  TestEntity entity_1{1};
  TestEntity entity_2{2};
  TestEntity entity_3{3};

  // Discover some entities.
  lost_entity_tracker.RecordFoundEntity(entity_1);
  lost_entity_tracker.RecordFoundEntity(entity_2);

  // Make sure none are lost on the first round.
  EXPECT_TRUE(lost_entity_tracker.ComputeLostEntities().empty());

  // Go through the next round only rediscovering one of our entities and
  // discovering an additional entity as well. Then, verify that only one entity
  // was lost after the check.
  lost_entity_tracker.RecordFoundEntity(entity_1);
  lost_entity_tracker.RecordFoundEntity(entity_3);
  typename LostEntityTracker<TestEntity>::EntitySet lost_entities =
      lost_entity_tracker.ComputeLostEntities();
  EXPECT_TRUE(lost_entities.find(entity_1) == lost_entities.end());
  EXPECT_TRUE(lost_entities.find(entity_2) != lost_entities.end());
  EXPECT_TRUE(lost_entities.find(entity_3) == lost_entities.end());
}

TEST(LostEntityTrackerTest, SameEntityMultipleCopies) {
  LostEntityTracker<TestEntity> lost_entity_tracker;
  TestEntity entity_1{1};
  TestEntity entity_1_copy{1};

  // Discover an entity.
  lost_entity_tracker.RecordFoundEntity(entity_1);

  // Make sure none are lost on the first round.
  EXPECT_TRUE(lost_entity_tracker.ComputeLostEntities().empty());

  // Rediscover the same entity, but through a copy of it.
  lost_entity_tracker.RecordFoundEntity(entity_1_copy);

  // Make sure none are lost on the second round.
  EXPECT_TRUE(lost_entity_tracker.ComputeLostEntities().empty());

  // Go through a round without rediscovering any entities and verify that we
  // lost an entity equivalent to both copies of it.
  typename LostEntityTracker<TestEntity>::EntitySet lost_entities =
      lost_entity_tracker.ComputeLostEntities();
  EXPECT_EQ(lost_entities.size(), 1);
  EXPECT_TRUE(lost_entities.find(entity_1) != lost_entities.end());
  EXPECT_TRUE(lost_entities.find(entity_1_copy) != lost_entities.end());
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
