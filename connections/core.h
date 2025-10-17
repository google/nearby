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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/service_controller_router.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/params.h"
#include "connections/payload.h"
#include "connections/v3/advertising_options.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/connections_device_provider.h"
#include "connections/v3/discovery_options.h"
#include "connections/v3/listeners.h"
#include "connections/v3/listening_result.h"
#include "internal/analytics/event_logger.h"
#include "internal/interop/device.h"
#include "internal/interop/device_provider.h"

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

  //******************************* V3 *******************************
  // NOTE: Do NOT mix with the V1 APIs above, this might result in undefined
  // behavior!

  // Starts advertising an endpoint for a local app.
  //
  // service_id - An identifier to advertise your app to other endpoints.
  //              This can be an arbitrary string, so long as it uniquely
  //              identifies your service. A good default is to use your
  //              app's package name.
  // advertising_options - The options for advertising.
  // local_device        - The local device for use when advertising when a
  //                       `DeviceProvider` has not been registered.
  // listener            - The set of callbacks for connection events.
  // callback - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if advertising started successfully.
  //     Status::STATUS_ALREADY_ADVERTISING if the app is already advertising.
  //     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
  //         connected to remote endpoints; call StopAllEndpoints first.
  void StartAdvertisingV3(absl::string_view service_id,
                          const v3::AdvertisingOptions& advertising_options,
                          const NearbyDevice& local_device,
                          v3::ConnectionListener listener,
                          ResultCallback callback);

  // Starts advertising an endpoint for a local app.
  // Should only be used when DeviceProvider has been registered with this
  // Nearby Connections instance. Otherwise, use the
  // StartAdvertisingV3(service_id, advertising_options, local_device, callback)
  // function above.
  //
  // service_id - An identifier to advertise your app to other endpoints.
  //              This can be an arbitrary string, so long as it uniquely
  //              identifies your service. A good default is to use your
  //              app's package name.
  // advertising_options - The options for advertising.
  // callback - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if advertising started successfully.
  //     Status::STATUS_ALREADY_ADVERTISING if the app is already advertising.
  //     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
  //         connected to remote endpoints; call StopAllEndpoints first.
  void StartAdvertisingV3(absl::string_view service_id,
                          const v3::AdvertisingOptions& advertising_options,
                          v3::ConnectionListener listener,
                          ResultCallback callback);

  // Stops advertising a local endpoint. Should be called after calling
  // StartAdvertising, as soon as the application no longer needs to advertise
  // itself or goes inactive. Payloads can still be sent to connected
  // endpoints after advertising ends.
  //
  // result_cb - to access the status of the operation when available.
  void StopAdvertisingV3(ResultCallback result_cb);

  // Starts discovery for remote endpoints with the specified service ID.
  //
  // service_id  - The ID for the service to be discovered, as specified in
  //              the corresponding call to StartAdvertising.
  // discovery_options    - The options for discovery.
  // listener_cb - A callback notified when a remote endpoint is discovered.
  // result_cb   - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if discovery started successfully.
  //     Status::STATUS_ALREADY_DISCOVERING if the app is already
  //         discovering the specified service.
  //     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
  //         connected to remote endpoints; call StopAllEndpoints first.
  void StartDiscoveryV3(absl::string_view service_id,
                        const v3::DiscoveryOptions& discovery_options,
                        v3::DiscoveryListener listener_cb,
                        ResultCallback callback);

  // Stops discovery for remote endpoints, after a previous call to
  // StartDiscovery, when the client no longer needs to discover endpoints or
  // goes inactive. Payloads can still be sent to connected endpoints after
  // discovery ends.
  //
  // result_cb - to access the status of the operation when available.
  void StopDiscoveryV3(ResultCallback result_cb);

  // Starts listening for incoming connections.
  //
  // options - The options for listening for a connection.
  //   - options.listening_device_type will be checked to make sure it is one of
  //   kConnectionsDevice or kPresenceDevice before starting the operation.
  // service_id - The service ID to listen for.
  // listener_cb - The connection listener to broadcast any updates.
  // result_cb - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if listening started successfully.
  //     Status::STATUS_ALREADY_LISTENING if the app is already listening.
  //     Status::STATUS_ALREADY_ADVERTISING if the app is already advertising.
  //     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently connected
  //         to remote endpoints; call StopAllEndpoints first.
  void StartListeningForIncomingConnectionsV3(
      const v3::ConnectionListeningOptions& options,
      absl::string_view service_id, v3::ConnectionListener listener_cb,
      v3::ListeningResultListener result_cb);

  // Stops listening for incoming connections. Should be called after
  // calling StartListeningForIncomingConnections.
  void StopListeningForIncomingConnectionsV3();

  // Sends a request to connect to a remote endpoint.
  //
  // local_device  - The local device information which will be shown on the
  //                 remote endpoint. Used only when a DeviceProvider is not
  //                 registered.
  // remote_device - The remote device to which a connection request will be
  //                sent. Should match the value provided in a call to
  //                DiscoveryListener::endpoint_found_cb()
  // connection_options - Connection options for the new connection if both
  //                      sides accept.
  // connection_cb - The callback to be notified on connection events.
  // result_cb    - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if the connection request was sent.
  //     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT if the app already
  //         has a connection to the specified endpoint.
  //     Status::STATUS_RADIO_ERROR if we failed to connect because of an
  //         issue with Bluetooth/WiFi.
  //     Status::STATUS_ERROR if we failed to connect for any other reason.
  void RequestConnectionV3(const NearbyDevice& local_device,
                           const NearbyDevice& remote_device,
                           ConnectionOptions connection_options,
                           v3::ConnectionListener connection_cb,
                           ResultCallback result_cb);

  // Sends a request to connect to a remote endpoint.
  //
  // remote_device - The remote device to which a connection request will be
  //                sent. Should match the value provided in a call to
  //                DiscoveryListener::endpoint_found_cb()
  // connection_options - Connection options for the new connection if both
  //                      sides accept.
  // connection_cb - The callback to be notified on connection events.
  // result_cb    - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if the connection request was sent.
  //     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT if the app already
  //         has a connection to the specified endpoint.
  //     Status::STATUS_RADIO_ERROR if we failed to connect because of an
  //         issue with Bluetooth/WiFi.
  //     Status::STATUS_ERROR if we failed to connect for any other reason.
  void RequestConnectionV3(const NearbyDevice& remote_device,
                           ConnectionOptions connection_options,
                           v3::ConnectionListener connection_cb,
                           ResultCallback result_cb);

  // Accepts a connection to a remote endpoint. This method must be called
  // before Payloads can be exchanged with the remote endpoint.
  //
  // remote_device - The remote device. Should match the value provided in a
  //                 call to ConnectionListener::OnConnectionInitiated.
  //
  // listener_cb - A callback for payloads exchanged with the remote endpoint.
  //
  // result_cb   - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if the connection request was accepted.
  //     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT if the app already.
  //         has a connection to the specified endpoint.
  //     Status::STATUS_ENDPOINT_UNKNOWN if the app doesn't currently have a
  //         pending connection to the remote device.
  void AcceptConnectionV3(const NearbyDevice& remote_device,
                          v3::PayloadListener listener_cb,
                          ResultCallback result_cb);

  // Rejects a connection to a remote endpoint.
  //
  // remote_device - The device for the remote endpoint. Should match the
  //               value provided in a call to
  //               v3::ConnectionListener::OnConnectionInitiated().
  // result_cb   - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK} if the connection request was rejected.
  //     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT} if the app already
  //         has a connection to the specified endpoint.
  void RejectConnectionV3(const NearbyDevice& remote_device,
                          ResultCallback result_cb);

  // Sends a Payload to a remote device. Payloads can only be sent to remote
  // devices once a notice of connection acceptance has been delivered via
  // v3::ConnectionListener::OnConnectionResult().
  //
  // remote_device - The remote device to which the payload should be sent.
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
  void SendPayloadV3(const NearbyDevice& remote_device, Payload payload,
                     ResultCallback result_cb);

  // Cancels a Payload currently in-flight to or from remote endpoint(s).
  //
  // remote_device - The remote device with which the payload is being
  //                 exchanged.
  // payload_id - The identifier for the Payload to be canceled.
  // result_cb  - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK if none of the above errors occurred.
  void CancelPayloadV3(const NearbyDevice& remote_device, int64_t payload_id,
                       ResultCallback result_cb);

  // Disconnects from a remote endpoint. {@link Payload}s can no longer be sent
  // to or received from the endpoint after this method is called.
  //
  // remote_device - The remote device to disconnect from.
  // result_cb   - to access the status of the operation when available.
  //   Possible status codes include:
  //     Status::STATUS_OK - finished successfully.
  void DisconnectFromDeviceV3(const NearbyDevice& remote_device,
                              ResultCallback result_cb);

  // Disconnects from, and removes all traces, of all connected and/or
  // discovered endpoints. This call is expected to be preceded by a call to
  // StopAdvertising() or StopDiscovery() as needed. After calling
  // StopAllDevices(), no further operations with remote endpoints will be
  // possible until a new call to one of StartAdvertising() or StartDiscovery().
  //
  // result_cb - To access the status of the operation when available.
  void StopAllDevicesV3(ResultCallback result_cb);

  // Sends a request to initiate connection bandwidth upgrade.
  //
  // remote_device - The identifier for the remote device which will be
  //               switching to a higher connection data rate and possibly
  //               different wireless protocol. On success, calls
  //               ConnectionListener::bandwidth_changed_cb().
  // result_cb   - to access the status of the operation when available.
  //  Possible status codes include:
  //    Status::STATUS_OK - finished successfully.
  void InitiateBandwidthUpgradeV3(const NearbyDevice& remote_device,
                                  ResultCallback result_cb);

  // Updates AdvertisingOptions. It compares the old AdvertisingOptions and the
  // new AdvertisingOptions to start/stop each advertising medium.
  //
  // advertising_options - The new advertising options a client wishes to use.
  // result_cb - to access the status of the operation when available.
  void UpdateAdvertisingOptionsV3(absl::string_view service_id,
                                  v3::AdvertisingOptions advertising_options,
                                  ResultCallback result_cb);

  // Updates DiscoveryOptions. It compares the old DiscoveryOptions and the new
  // DiscoveryOptions to start/stop each discovery medium.
  //
  // discovery_options - The new discovery options a client wishes to use.
  // result_cb - to access the status of the operation when available.
  void UpdateDiscoveryOptionsV3(absl::string_view service_id,
                                v3::DiscoveryOptions discovery_options,
                                ResultCallback result_cb);

  // Registers a DeviceProvider to provide functionality for Nearby Connections
  // to interact with the DeviceProvider for retrieving the local device.
  void RegisterDeviceProvider(NearbyDeviceProvider* provider) {
    client_.RegisterDeviceProvider(provider);
  }

  // Like RegisterDeviceProvider(NearbyDeviceProvider*) above, but for
  // a Connections device provider, so Connections can manage the lifetime of
  // this device provider. If an external provider is registered, this provider
  // will be ignored.
  void RegisterConnectionsDeviceProvider(
      std::unique_ptr<v3::ConnectionsDeviceProvider> provider) {
    client_.RegisterConnectionsDeviceProvider(std::move(provider));
  }

 private:
  ClientProxy client_;
  ServiceControllerRouter* router_ = nullptr;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_CORE_H_
