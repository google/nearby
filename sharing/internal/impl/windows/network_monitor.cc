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

#include "sharing/internal/impl/windows/network_monitor.h"

#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Networking.Connectivity.h"

namespace nearby::sharing::windows {
namespace {

using ::winrt::Windows::Networking::Connectivity::ConnectionProfile;
using ::winrt::Windows::Networking::Connectivity::NetworkConnectivityLevel;
using ::winrt::Windows::Networking::Connectivity::NetworkInformation;

struct Connectivity {
  bool lan = false;
  bool internet = false;
};

Connectivity QueryConnectivity() {
  Connectivity connectivity;
  try {
    for (const ConnectionProfile& profile :
         NetworkInformation::GetConnectionProfiles()) {
      if (profile.GetNetworkConnectivityLevel() !=
          NetworkConnectivityLevel::None) {
        connectivity.lan = true;
        break;
      }
    }
    ConnectionProfile internet =
        NetworkInformation::GetInternetConnectionProfile();
    connectivity.internet =
        internet != nullptr && internet.GetNetworkConnectivityLevel() ==
                                   NetworkConnectivityLevel::InternetAccess;
  } catch (const winrt::hresult_error& error) {
    LOG(WARNING) << "Failed to query connection profiles: "
                 << winrt::to_string(error.message());
  }
  return connectivity;
}

}  // namespace

NetworkMonitor::NetworkMonitor(
    absl::AnyInvocable<void(bool)> lan_connected_callback,
    absl::AnyInvocable<void(bool)> internet_connected_callback)
    : nearby::api::NetworkMonitor(std::move(lan_connected_callback),
                                  std::move(internet_connected_callback)) {
  Connectivity connectivity = QueryConnectivity();
  {
    absl::MutexLock lock(mutex_);
    lan_connected_ = connectivity.lan;
    internet_connected_ = connectivity.internet;
  }
  status_changed_token_ = NetworkInformation::NetworkStatusChanged(
      [this](const winrt::Windows::Foundation::IInspectable&) { Refresh(); });
}

NetworkMonitor::~NetworkMonitor() {
  NetworkInformation::NetworkStatusChanged(status_changed_token_);
}

bool NetworkMonitor::IsLanConnected() {
  absl::MutexLock lock(mutex_);
  return lan_connected_;
}

bool NetworkMonitor::IsInternetConnected() {
  absl::MutexLock lock(mutex_);
  return internet_connected_;
}

void NetworkMonitor::Refresh() {
  Connectivity connectivity = QueryConnectivity();
  bool lan_changed;
  bool internet_changed;
  {
    absl::MutexLock lock(mutex_);
    lan_changed = lan_connected_ != connectivity.lan;
    internet_changed = internet_connected_ != connectivity.internet;
    lan_connected_ = connectivity.lan;
    internet_connected_ = connectivity.internet;
  }
  // Callbacks run outside the lock; they may query this monitor.
  if (lan_changed && lan_connected_callback_) {
    lan_connected_callback_(connectivity.lan);
  }
  if (internet_changed && internet_connected_callback_) {
    internet_connected_callback_(connectivity.internet);
  }
}

}  // namespace nearby::sharing::windows
