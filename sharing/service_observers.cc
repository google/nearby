// Copyright 2025 Google LLC
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

#include "sharing/service_observers.h"

#include "absl/synchronization/mutex.h"
#include "sharing/nearby_sharing_service.h"

namespace nearby::sharing {

using ::absl::Seconds;

// An RAII class that tracks the number of inflight observer notifications.
// This is used to ensure that observers are not destroyed until all inflight
// notifications have completed.
class ObserverNotifyTracker {
 public:
  explicit ObserverNotifyTracker(ServiceObservers& service_observers)
      : service_observers_(service_observers) {
    service_observers_.BeginNotify();
  }
  ~ObserverNotifyTracker() { service_observers_.EndNotify(); }

 private:
  ServiceObservers& service_observers_;
};

void ServiceObservers::AddObserver(NearbySharingService::Observer* observer) {
  observers_.AddObserver(observer);
}

void ServiceObservers::RemoveObserver(
    NearbySharingService::Observer* observer) {
  if (!observers_.HasObserver(observer)) {
    return;
  }
  observers_.RemoveObserver(observer);
  // Wait an arbitrary amount of time (1s) for inflight callbacks to complete.
  absl::MutexLock lock(mutex_);
  mutex_.AwaitWithTimeout(absl::Condition(
      +[](int* in_flight_notify_count) { return *in_flight_notify_count == 0; },
      &in_flight_notify_count_), Seconds(1));
}

void ServiceObservers::Clear() {
  observers_.Clear();
}

bool ServiceObservers::HasObserver(NearbySharingService::Observer* observer) {
  return observers_.HasObserver(observer);
}

void ServiceObservers::BeginNotify() {
  absl::MutexLock lock(mutex_);
  ++in_flight_notify_count_;
}

void ServiceObservers::EndNotify() {
  absl::MutexLock lock(mutex_);
  --in_flight_notify_count_;
}

void ServiceObservers::NotifyHighVisibilityChangeRequested() {
  ObserverNotifyTracker notify_tracker(*this);
  for (auto& observer : observers_.GetObservers()) {
    observer->OnHighVisibilityChangeRequested();
  }
}

void ServiceObservers::NotifyHighVisibilityChanged(bool in_high_visibility) {
  ObserverNotifyTracker notify_tracker(*this);
  for (auto& observer : observers_.GetObservers()) {
    observer->OnHighVisibilityChanged(in_high_visibility);
  }
}

void ServiceObservers::NotifyStartAdvertisingFailure() {
  ObserverNotifyTracker notify_tracker(*this);
  for (auto& observer : observers_.GetObservers()) {
    observer->OnStartAdvertisingFailure();
  }
}

void ServiceObservers::NotifyStartDiscoveryResult(bool success) {
  ObserverNotifyTracker notify_tracker(*this);
  for (auto& observer : observers_.GetObservers()) {
    observer->OnStartDiscoveryResult(success);
  }
}

void ServiceObservers::NotifyBluetoothStatusChanged(
    NearbySharingService::Observer::AdapterState state) {
  ObserverNotifyTracker notify_tracker(*this);
  for (auto& observer : observers_.GetObservers()) {
    observer->OnBluetoothStatusChanged(state);
  }
}

void ServiceObservers::NotifyLanStatusChanged(
    NearbySharingService::Observer::AdapterState state) {
  ObserverNotifyTracker notify_tracker(*this);
  for (auto& observer : observers_.GetObservers()) {
    observer->OnLanStatusChanged(state);
  }
}

void ServiceObservers::NotifyIrrecoverableHardwareErrorReported() {
  ObserverNotifyTracker notify_tracker(*this);
  for (auto& observer : observers_.GetObservers()) {
    observer->OnIrrecoverableHardwareErrorReported();
  }
}

void ServiceObservers::NotifyCredentialError() {
  ObserverNotifyTracker notify_tracker(*this);
  for (auto& observer : observers_.GetObservers()) {
    observer->OnCredentialError();
  }
}

}  // namespace nearby::sharing
