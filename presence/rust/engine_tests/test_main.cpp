// Copyright 2024 Google LLC
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

// Provide a main() by Catch - only one main per cpp file
#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include "engine.h"

TEST_CASE( "Presence Engine", "DiscoveryEngineRequest Echo" ) {
    const int conditionCnt = 2;
    DiscoveryCondition conditions[conditionCnt] = {
        {10, // action
         IdentityType::Private, MeasurementAccuracy::CoarseAccuracy},
        {11, // action
         IdentityType::Trusted, MeasurementAccuracy::BestAvailable},
    };
    DiscoveryConditionList condition_list = {
       &conditions[0],
       conditionCnt,
    };

    DiscoveryEngineRequest request = {
      7, // priority
      condition_list, // condition
    };

    auto request_echoed = echo_request(&request);
    REQUIRE(request_echoed->priority == 7);
    REQUIRE(request_echoed->conditions.count == 2);
    REQUIRE(request_echoed->conditions.items[0].action == 10);
    REQUIRE(request_echoed->conditions.items[0].identity_type == IdentityType::Private);
    REQUIRE(request_echoed->conditions.items[0].identity_type == IdentityType::Private);
    REQUIRE(request_echoed->conditions.items[0].measurement_accuracy == MeasurementAccuracy::CoarseAccuracy);
    REQUIRE(request_echoed->conditions.items[1].action == 11);
    REQUIRE(request_echoed->conditions.items[1].identity_type == IdentityType::Trusted);
    REQUIRE(request_echoed->conditions.items[1].measurement_accuracy == MeasurementAccuracy::BestAvailable);

    free_engine_request(request_echoed);
}