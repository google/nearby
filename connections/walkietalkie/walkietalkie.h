#ifndef WALKIETALKIE_CLIENT_H_
#define WALKIETALKIE_CLIENT_H_

#include <shared_mutex>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/core.h"
#include "connections/discovery_options.h"
#include "connections/implementation/analytics/throughput_recorder.h"
#include "connections/implementation/service_controller_router.h"
#include "connections/listeners.h"
#include "connections/params.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "connections/walkietalkie/audio_player.h"
#include "internal/interop/device_provider.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/payload_id.h"

class WalkieTalkie {
  const constexpr static char* walkietalkie_service_id =
      "com.google.location.nearby.apps.walkietalkie.manual.SERVICE_ID";

 public:
  WalkieTalkie(const WalkieTalkie&) = delete;
  WalkieTalkie(WalkieTalkie&&) = delete;
  WalkieTalkie& operator=(const WalkieTalkie&) = delete;
  WalkieTalkie& operator=(WalkieTalkie&&) = delete;

  explicit WalkieTalkie(
      std::unique_ptr<nearby::connections::ServiceControllerRouter> router)
      : router_(std::move(router)),
        core_(std::make_unique<nearby::connections::Core>(router_.get())),
        player_(nullptr),
        pending_payloads_({}) {}
  ~WalkieTalkie() {
    std::unique_lock lock(player_mutex_);
    if (player_ != nullptr) player_->stop();
  }

  static std::string endpoint_name() {
    std::stringstream name;
    for (auto i = 0; i < 5; i++) {
      name << (std::rand() % 10);
    }

    return name.str();
  }

  void start() {
    nearby::connections::AdvertisingOptions advertising;
    advertising.strategy = nearby::connections::Strategy::kP2pStar;
    advertising.device_info = "walkietalkie";
    advertising.allowed.SetAll(false);
    advertising.allowed.wifi_direct = false;
    advertising.allowed.wifi_hotspot = false;
    advertising.allowed.wifi_lan = true ;
    advertising.allowed.ble = false;
    advertising.allowed.bluetooth = true;
    advertising.low_power = false;
    advertising.auto_upgrade_bandwidth = false;

    auto local_endpoint_info = nearby::ByteArray(endpoint_name());

    core_->StartAdvertising(
        walkietalkie_service_id, advertising.CompatibleOptions(),
        createConnReqInfo(local_endpoint_info),
        [](nearby::connections::Status status) {
          LOG(INFO) << "Advertising status: " << status.ToString();
        });

    nearby::connections::DiscoveryOptions discovery;
    discovery.strategy = advertising.strategy;
    discovery.allowed = advertising.allowed;
    discovery.low_power = false;

    nearby::connections::DiscoveryListener discovery_listener;
    discovery_listener.endpoint_found_cb =
        [this, local_endpoint_info](const std::string& remote_endpoint_id,
                                    const nearby::ByteArray& endpoint_info,
                                    const std::string& service_id) {
          LOG(INFO) << "Found " << service_id << " with endpoint id "
                    << remote_endpoint_id;
          nearby::connections::ConnectionOptions conn_options;
          conn_options.allowed.SetAll(false);
          conn_options.allowed.bluetooth = true;
          conn_options.allowed.wifi_hotspot = false;
          conn_options.allowed.wifi_lan = true;
	        conn_options.auto_upgrade_bandwidth = false;
          conn_options.connection_info.local_endpoint_info =
              local_endpoint_info;
          
          if (service_id == walkietalkie_service_id) {
            LOG(INFO) << "Requesting a connection to " << service_id
                      << " on remote endpoint id " << remote_endpoint_id;
            core_->RequestConnection(
                remote_endpoint_id, createConnReqInfo(endpoint_info),
                conn_options, [](nearby::connections::Status status) {
                  LOG(INFO)
                      << "Request connection status: " << status.ToString();
                });
          }
        };
    core_->StartDiscovery(
        walkietalkie_service_id, discovery.CompatibleOptions(),
        std::move(discovery_listener), [](nearby::connections::Status status) {
          LOG(INFO) << "Discovery status: " << status.ToString();
        });
  }

  nearby::connections::ConnectionRequestInfo createConnReqInfo(
      const nearby::ByteArray& endpoint_info) {
    nearby::connections::ConnectionRequestInfo con_req_info;
    con_req_info.endpoint_info = endpoint_info;
    con_req_info.listener.initiated_cb =
        [&](const std::string& id,
            const nearby::connections::ConnectionResponseInfo& info) {
          connection_initiated(id, info);
        };
    con_req_info.listener.accepted_cb = [&](const std::string& id) {
      connection_accepted(id);
    };
    con_req_info.listener.disconnected_cb = [&](const std::string& id) {
      disconnected(id);
    };
    return con_req_info;
  }

  void add_pending_remote_endpoint(const std::string& endpoint_id,
                                   nearby::ByteArray endpoint_info) {
    std::unique_lock<std::shared_mutex> lock(pending_conns_mutex_);
    pending_conns_.insert({endpoint_id, std::move(endpoint_info)});
  }

  void remove_pending_remote_endpoint(const std::string& endpoint_id) {
    std::unique_lock lock(pending_conns_mutex_);
    pending_conns_.erase(endpoint_id);
  }

  void connection_initiated(
      const std::string& endpoint_id,
      const nearby::connections::ConnectionResponseInfo& info) {
    auto i = info;
    LOG(INFO) << "connected initiated, endpoint id: '" << endpoint_id
              << "', remote endpoint id: '"
              << info.remote_endpoint_info.string_data() << "'";

    add_pending_remote_endpoint(endpoint_id, info.remote_endpoint_info);

    struct nearby::connections::PayloadListener listener;
    listener.payload_cb = [&](absl::string_view endpoint_id,
                              nearby::connections::Payload payload) {
      if (payload.GetType() == nearby::analytics::PayloadType::kStream) {
        auto id = payload.GetId();
        auto shared =
            std::make_shared<nearby::connections::Payload>(std::move(payload));
        {
          std::unique_lock lock(pending_payloads_mutex_);
          pending_payloads_.emplace(std::pair{std::string(endpoint_id), id},
                                    shared);
        }
      }
    };

    listener.payload_progress_cb =
        [&](absl::string_view endpoint_id,
            const nearby::connections::PayloadProgressInfo& info) {
          auto key = std::pair{std::string(endpoint_id), info.payload_id};
          switch (info.status) {
            case nearby::connections::PayloadProgressInfo::Status::kSuccess: {
              std::shared_ptr<nearby::connections::Payload> payload = nullptr;
              {
                std::unique_lock payloads_lock(pending_payloads_mutex_);
                payload = pending_payloads_[key];
                pending_payloads_.erase(key);
              }

              std::unique_lock player_lock(player_mutex_);
              if (player_ == nullptr) player_ = std::make_unique<AudioPlayer>();

              player_->start(payload, info.bytes_transferred);
            } break;
            case nearby::connections::PayloadProgressInfo::Status::kCanceled:
            case nearby::connections::PayloadProgressInfo::Status::kFailure: {
              std::unique_lock lock(pending_payloads_mutex_);
              pending_payloads_.erase(key);
            }
          }
        };
    core_->AcceptConnection(endpoint_id, std::move(listener),
                            [&](nearby::connections::Status status) {
                              LOG(INFO) << "AcceptConnection status: "
                                        << status.ToString();
                            });
  }

  void connection_accepted(const std::string& endpoint_id) {
    LOG(INFO) << "connection accepted, remote endpoint id: '" << endpoint_id
              << "'";
    remove_pending_remote_endpoint(endpoint_id);
    core_->StopAdvertising([](nearby::connections::Status){});
    // core_->StopDiscovery([](nearby::connections::Status) {});
  }

  void disconnected(const std::string& endpoint_id) {
    LOG(INFO) << "disconnected, remote endpoint id: " << endpoint_id;
    remove_pending_remote_endpoint(endpoint_id);
  }

  // Discovery callbacks

 private:
  std::unique_ptr<nearby::connections::ServiceControllerRouter> router_;

  std::unique_ptr<nearby::connections::Core> core_;

  std::mutex player_mutex_;
  std::unique_ptr<AudioPlayer> player_;

  std::shared_mutex pending_conns_mutex_;
  std::unordered_map<std::string, nearby::ByteArray> pending_conns_;

  std::mutex pending_payloads_mutex_;
  std::map<std::pair<std::string, nearby::PayloadId>,
           std::shared_ptr<nearby::connections::Payload>>
      pending_payloads_;
};
#endif
