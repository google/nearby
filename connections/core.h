// Copyright 2021 Google LLC
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

#ifndef CORE_CORE_H_
#define CORE_CORE_H_

#include <functional>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "connections/device_provider.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/service_controller.h"
#include "connections/implementation/service_controller_router.h"
#include "connections/listeners.h"
#include "connections/params.h"
#include "internal/analytics/event_logger.h"
#include "internal/device.h"

namespace nearby {
namespace connections {

// This class defines the API of the Nearby Connections Core library.
class Core {
 public:
  explicit Core(ServiceControllerRouter* router);
  // Client needs to call this constructor if analytics logger is needed.
  Core(::nearby::analytics::EventLogger* event_logger,
       ServiceControllerRouter* router)
      : client_(event_logger), router_(router) {}
  ~Core();
  Core(Core&&);
  Core& operator=(Core&&);

  // Starts advertising an endpoint for a local app.
  //
  // service_id - An identifier to advertise your app to other endpoints.
  //              This can be an arbitrary string, so long as it uniquely
  //              identifies your service. A good default is to use your
  //              app's package name.
  // advertising_options - The options for advertising.
  // info       - Connection parameters:
  // > name     - A human readable name for this endpoint, to appear on
  //              other devices.
  // > listener - A callback notified when remote endpoints request a
  //              connection to this endpoint.
  // callback - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if advertising started successfully.
  //     Status::STATUS_ALREADY_ADVERTISING if the app is already advertising.
  //     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
  //         connected to remote endpoints; call StopAllEndpoints first.
  void StartAdvertising(absl::string_view service_id,
                        AdvertisingOptions advertising_options,
                        ConnectionRequestInfo info, ResultCallback callback);

  // Stops advertising a local endpoint. Should be called after calling
  // StartAdvertising, as soon as the application no longer needs to advertise
  // itself or goes inactive. Payloads can still be sent to connected
  // endpoints after advertising ends.
  //
  // result_cb - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if none of the above errors occurred.
  void StopAdvertising(ResultCallback callback);

  // Starts discovery for remote endpoints with the specified service ID.
  //
  // service_id - The ID for the service to be discovered, as specified in
  //              the corresponding call to StartAdvertising.
  // listener   - A callback notified when a remote endpoint is discovered.
  // discovery_options    - The options for discovery.
  // result_cb  - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if discovery started successfully.
  //     Status::STATUS_ALREADY_DISCOVERING if the app is already
  //         discovering the specified service.
  //     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
  //         connected to remote endpoints; call StopAllEndpoints first.
  void StartDiscovery(absl::string_view service_id,
                      DiscoveryOptions discovery_options,
                      DiscoveryListener listener, ResultCallback callback);

  // Stops discovery for remote endpoints, after a previous call to
  // StartDiscovery, when the client no longer needs to discover endpoints or
  // goes inactive. Payloads can still be sent to connected endpoints after
  // discovery ends.
  //
  // result_cb - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if none of the above errors occurred.
  void StopDiscovery(ResultCallback callback);

  // Invokes the discovery callback from a previous call to StartDiscovery()
  // with the given endpoint info. The previous call to StartDiscovery() must
  // have been passed ConnectionOptions with is_out_of_band_connection == true.
  //
  // service_id            - The ID for the service to be discovered, as
  //                         specified in the corresponding call to
  //                         StartDiscovery().
  // metadata              - Metadata used in order to inject the endpoint.
  // result_cb             - to access the status of the operation when
  //                         available.
  //   Possible status codes include:
  //     Status::kSuccess if endpoint injection was attempted.
  //     Status::kError if endpoint_id, endpoint_info, or
  //         remote_bluetooth_mac_address are malformed.
  //     Status::kOutOfOrderApiCall if the app is not discovering.
  void InjectEndpoint(absl::string_view service_id,
                      OutOfBandConnectionMetadata metadata,
                      ResultCallback callback);

  // Sends a request to connect to a remote endpoint.
  //
  // endpoint_id - The identifier for the remote endpoint to which a
  //               connection request will be sent. Should match the value
  //               provided in a call to
  //               DiscoveryListener::endpoint_found_cb()
  // info        - Connection parameters:
  // > name      - A human readable name for the local endpoint, to appear on
  //               the remote endpoint.
  // > listener  - A callback notified when the remote endpoint sends a
  //               response to the connection request.
  // result_cb   - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if the connection request was sent.
  //     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT if the app already
  //         has a connection to the specified endpoint.
  //     Status::STATUS_RADIO_ERROR if we failed to connect because of an
  //         issue with Bluetooth/WiFi.
  //     Status::STATUS_ERROR if we failed to connect for any other reason.
  void RequestConnection(absl::string_view endpoint_id,
                         ConnectionRequestInfo info,
                         ConnectionOptions connection_options,
                         ResultCallback callback);

  // Accepts a connection to a remote endpoint. This method must be called
  // before Payloads can be exchanged with the remote endpoint.
  //
  // endpoint_id - The identifier for the remote endpoint. Should match the
  //               value provided in a call to
  //               ConnectionListener::onConnectionInitiated.
  // listener    - A callback for payloads exchanged with the remote endpoint.
  // result_cb   - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if the connection request was accepted.
  //     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT if the app already.
  //         has a connection to the specified endpoint.
  void AcceptConnection(absl::string_view endpoint_id, PayloadListener listener,
                        ResultCallback callback);

  // Rejects a connection to a remote endpoint.
  //
  // endpoint_id - The identifier for the remote endpoint. Should match the
  //               value provided in a call to
  //               ConnectionListener::onConnectionInitiated().
  // result_cb   - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK} if the connection request was rejected.
  //     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT} if the app already
  //         has a connection to the specified endpoint.
  void RejectConnection(absl::string_view endpoint_id, ResultCallback callback);

  // Sends a Payload to a remote endpoint. Payloads can only be sent to remote
  // endpoints once a notice of connection acceptance has been delivered via
  // ConnectionListener::onConnectionResult().
  //
  // endpoint_ids - Array of remote endpoint identifiers for the  to which the
  //                payload should be sent.
  // payload      - The Payload to be sent.
  // result_cb    - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OUT_OF_ORDER_API_CALL if the device has not first
  //         performed advertisement or discovery (to set the Strategy.
  //     Status::STATUS_ENDPOINT_UNKNOWN if there's no active (or pending)
  //         connection to the remote endpoint.
  //     Status::STATUS_OK if none of the above errors occurred. Note that this
  //         indicates that Nearby Connections will attempt to send the Payload,
  //         but not that the send has successfully completed yet. Errors might
  //         still occur during transmission (and at different times for
  //         different endpoints), and will be delivered via
  //         PayloadCallback#onPayloadTransferUpdate.
  void SendPayload(absl::Span<const std::string> endpoint_ids, Payload payload,
                   ResultCallback callback);

  // Cancels a Payload currently in-flight to or from remote endpoint(s).
  //
  // payload_id - The identifier for the Payload to be canceled.
  // result_cb  - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if none of the above errors occurred.
  void CancelPayload(std::int64_t payload_id, ResultCallback callback);

  // Disconnects from a remote endpoint. {@link Payload}s can no longer be sent
  // to or received from the endpoint after this method is called.
  //
  // endpoint_id - The identifier for the remote endpoint to disconnect from.
  // result_cb   - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK - finished successfully.
  void DisconnectFromEndpoint(absl::string_view endpoint_id,
                              ResultCallback callback);

  // Disconnects from, and removes all traces of, all connected and/or
  // discovered endpoints. This call is expected to be preceded by a call to
  // StopAdvertising or StartDiscovery as needed. After calling
  // StopAllEndpoints, no further operations with remote endpoints will be
  // possible until a new call to one of StartAdvertising() or StartDiscovery().
  //
  // result_cb - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK - finished successfully.
  void StopAllEndpoints(ResultCallback callback);

  // Sends a request to initiate connection bandwidth upgrade.
  //
  // endpoint_id - The identifier for the remote endpoint which will be
  //               switching to a higher connection data rate and possibly
  //               different wireless protocol. On success, calls
  //               ConnectionListener::bandwidth_changed_cb().
  // result_cb   - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK - finished successfully.
  void InitiateBandwidthUpgrade(absl::string_view endpoint_id,
                                ResultCallback callback);

  // Sets a custom save path. Defaults to the download path.
  //
  // path - The path where the received files will be saved to.
  void SetCustomSavePath(absl::string_view path, ResultCallback callback);

  // Gets the local endpoint generated by Nearby Connections.
  std::string GetLocalEndpointId() { return client_.GetLocalEndpointId(); }

  std::string Dump();

  //******************************* V2 *******************************
  void RequestConnectionV2(const NearbyDevice& device,
                           const ConnectionRequestInfo& info,
                           ConnectionOptions& connection_options,
                           ResultCallback callback);

  void AcceptConnectionV2(const NearbyDevice& device,
                          const PayloadListener& listener,
                          ResultCallback callback);

  void RejectConnectionV2(const NearbyDevice& device, ResultCallback callback);

  // Span is being used here as we will not be modifying this block of memory.
  // We also do not need to own this block of memory, so we can use Span.
  // We are using NearbyDevice* so as to not lose attributes when using a
  // vector-like structure of NearbyDevice, as object information is stripped if
  // using NearbyDevice or NearbyDevice&.
  void SendPayloadV2(absl::Span<const NearbyDevice*> devices,
                     const Payload& payload, ResultCallback callback);

  void DisconnectFromDeviceV2(const NearbyDevice& device,
                              ResultCallback callback);

  void InitiateBandwidthUpgradeV2(const NearbyDevice& device,
                                  ResultCallback callback);

  void RegisterDeviceProvider(const NearbyDeviceProvider& provider);

 private:
  ClientProxy client_;
  ServiceControllerRouter* router_ = nullptr;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_CORE_H_
