// Provide a main() by Catch - only one main per cpp file
#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include "engine.h"

TEST_CASE( "Presence Engine", "DiscoveryEngineRequest Echo" ) {
    const int conditionCnt = 2;
    DiscoveryCondition conditions[conditionCnt] = {
       {1, 2, 3}, // action, identity type, accuracy.
       {4, 5, 6},
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
    REQUIRE(request_echoed->conditions.items[0].action == 1);
    REQUIRE(request_echoed->conditions.items[1].action == 4);

    free_engine_request(request_echoed);
}