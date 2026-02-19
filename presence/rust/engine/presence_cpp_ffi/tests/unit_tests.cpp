// Provide a main() by Catch - only one main per cpp file
#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "../cpp/rust_to_c.hpp"
#include "../presence.h"

TEST_CASE("Test presence_ble_scan_request", "rust_to_c") {
  int expected_priority = 1;
  int expected_action_zero = 0;
  int expected_action_one = 1;
  struct PresenceBleScanRequest
      * request = presence_ble_scan_request_new(expected_priority);
  presence_ble_scan_request_add_action(request, expected_action_zero);
  presence_ble_scan_request_add_action(request, expected_action_one);
  REQUIRE(request->priority == expected_priority);
  REQUIRE(request->actions.size() == 2);
  REQUIRE(request->actions[0] == expected_action_zero);
  REQUIRE(request->actions[1] == expected_action_one);
}

TEST_CASE("Test presence_discovery_result", "rust_to_c") {
  enum PresenceMedium expected_medium = PresenceMedium::BLE;
  int expected_action_zero = 0;
  int expected_action_one = 1;
  struct PresenceDiscoveryResult
      * result = presence_discovery_result_new(expected_medium);
  presence_discovery_result_add_action(result, expected_action_zero);
  presence_discovery_result_add_action(result, expected_action_one);
  REQUIRE(result->medium == expected_medium);
  REQUIRE(result->device.actions.size() == 2);
  REQUIRE(result->device.actions[0] == expected_action_zero);
  REQUIRE(result->device.actions[1] == expected_action_one);
}