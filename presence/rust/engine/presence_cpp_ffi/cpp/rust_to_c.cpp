#include <stdio.h>
#include <stdlib.h>
#include "../presence.h"
#include "rust_to_c.hpp"

struct PresenceBleScanRequest* presence_ble_scan_request_new(int priority) {
  PresenceBleScanRequest* request = new PresenceBleScanRequest();
  request->priority = priority;
  return request;
}

void presence_ble_scan_request_add_action(PresenceBleScanRequest* request,
                                          int action) {
  request->actions.push_back(action);
}

void presence_ble_scan_request_free(struct PresenceBleScanRequest* request) {
  delete request;
}

struct PresenceDiscoveryResult* presence_discovery_result_new(enum PresenceMedium medium) {
  // struct PresenceDevice presence_device = { .actions = NULL, .actions_size = 20 };
  // PresenceDiscoveryResult *result = (PresenceDiscoveryResult*)malloc(sizeof(PresenceDiscoveryResult));
  PresenceDiscoveryResult* result = new PresenceDiscoveryResult();
  result->medium = medium;
  return result;
}

void presence_discovery_result_add_action(PresenceDiscoveryResult* result,
                                          int action) {
  result->device.actions.push_back(action);
}