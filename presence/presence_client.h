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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_H_

#include <functional>
#include <memory>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "presence/broadcast_request.h"
#include "presence/discovery_filter.h"
#include "presence/discovery_options.h"
#include "presence/listeners.h"
#include "presence/presence_device.h"
#include "presence/status.h"

namespace nearby {
namespace presence {

// The callback of stop broadcast for client to invoke later.
struct BroadcastSession {
  absl::AnyInvocable<void(Status)> stop_broadcast_callback;
};

/**
 * Interface for detecting and interacting with nearby devices that are also
 * part of the Presence ecosystem.
 */
class PresenceClient {
 public:
  /**
   * Discovery APIs.
   */

  /**
   * Starts a Nearby Presence discovery and registers {@link ResultCallback}
   * which will be invoked when a matching {@link PresenceDevice} is detected
   * by the service.
   *
   * <p>The {@link ResultCallback} is registered at the Nearby Presence service.
   * And a same callback can only be registered once.
   *
   * <p>The returned {@link Task} may contain the value {@link
   * PresenceStatusCodes#RESOLUTION_REQUIRED}. If that is the case, client
   * should call {@link Status#startResolutionForResult} to get the users
   * consents/permissions before retrying the request.
   */

  void StartDiscovery(const DiscoveryFilter& filter,
                      const DiscoveryOptions& options, ResultCallback callback);

  /**
   * Update the discovery filter for an ongoing discovery.
   *
   * <p>The returned {@link Task} may contain the value
   * {@link PresenceStatusCodes#RESOLUTION_REQUIRED}. If that is the case,
   * client should call {@link Status#startResolutionForResult} to get the users
   * consents/permissions before retrying the request.
   */
  void UpdateDiscoveryFilter(const DiscoveryFilter& filter,
                             ResultCallback callback);

  /**
   * Stops a Nearby Presence discovery and unregisters a {@link
   * DiscoveryCallback} which was previously registered with {@link
   * #startDiscovery}.
   *
   * <p>If the callback wasn't previously registered, then this will be silently
   * ignored. Presence stops discovery, if no client is requesting discovery.
   */
  void StopDiscovery(ResultCallback callback);

  /**
   * Gets all devices which Presence has recently detected to be within range
   * and matching a given {@link DiscoveryFilter}.
   *
   * <p>There may be devices within proximity which are not returned as part of
   * this method because they haven't yet been discovered by Presence's
   * scanning. Apps can call this method multiple times to get the most
   * up-to-date information about what is around, however calling it does not
   * initiate any scans to find more devices, scanning is handled out-of-band
   * from this API.
   *
   * <p>Alternatively, clients can use {@link #startDiscovery} to be notified
   * when a device is discovered nearby, if the client would like to retrieve
   * live devices and their ranging data.
   */
  std::vector<PresenceDevice> GetCachedDevices(const DiscoveryFilter& filter);

  // Advertising APIs.

  // Starts broadcasting an advertisement with attributes defined by `request`.
  // The advertisement is sent over BLE4.2 or BLE5.0, or both if they are
  // supported by the platform. The `callback` is invoked when the
  // advertisement has started.
  //
  // Returns a `BroadcastSession`, which can be used to stop the broadcast
  // later.
  std::unique_ptr<BroadcastSession> StartBroadcast(
      const BroadcastRequest& request, const ResultCallback& callback);
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_H_
