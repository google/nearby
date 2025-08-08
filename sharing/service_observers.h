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

#ifndef THIRD_PARTY_NEARBY_SHARING_SERVICE_OBSERVERS_H_
#define THIRD_PARTY_NEARBY_SHARING_SERVICE_OBSERVERS_H_

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/observer_list.h"
#include "sharing/nearby_sharing_service.h"

namespace nearby::sharing {

// A class that manages a list of NearbySharingService::Observers.
// This class is thread-safe.
//
// This class is used to ensure that observers are not destroyed until all
// inflight notifications have completed.
//
// Do not call RemoveObserver() from an Observer callback, otherwise a deadlock
// will occur.
// TODO(ftsui): Consider using a callback in RemoveObserver() to avoid deadlock.
class ServiceObservers {
 public:
  void AddObserver(NearbySharingService::Observer* observer);
  // Remove an observer from the list.
  // This method will block until all inflight callbacks to observers have
  // completed.
  void RemoveObserver(NearbySharingService::Observer* observer);

  void Clear();
  bool HasObserver(NearbySharingService::Observer* observer);

  void NotifyHighVisibilityChangeRequested();
  void NotifyHighVisibilityChanged(bool in_high_visibility);
  void NotifyStartAdvertisingFailure();
  void NotifyStartDiscoveryResult(bool success);
  void NotifyBluetoothStatusChanged(
      NearbySharingService::Observer::AdapterState state);
  void NotifyWifiStatusChanged(
      NearbySharingService::Observer::AdapterState state);
  void NotifyLanStatusChanged(
      NearbySharingService::Observer::AdapterState state);
  void NotifyIrrecoverableHardwareErrorReported();
  void NotifyCredentialError();

 private:
  friend class ObserverNotifyTracker;

  void BeginNotify();
  void EndNotify();

  ObserverList<NearbySharingService::Observer> observers_;
  absl::Mutex mutex_;
  int in_flight_notify_count_ ABSL_GUARDED_BY(mutex_) = 0;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_SERVICE_OBSERVERS_H_
