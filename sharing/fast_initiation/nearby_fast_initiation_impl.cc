// Copyright 2022 Google LLC
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

#include "sharing/fast_initiation/nearby_fast_initiation_impl.h"

#include <functional>
#include <memory>
#include <ostream>
#include <utility>

#include "sharing/fast_initiation/nearby_fast_initiation.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/fast_init_ble_beacon.h"
#include "sharing/internal/api/fast_initiation_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"

namespace nearby {
namespace sharing {
using ::nearby::api::FastInitiationManager;

NearbyFastInitiationImpl::Factory*
    NearbyFastInitiationImpl::Factory::test_factory_ = nullptr;

std::unique_ptr<NearbyFastInitiation> NearbyFastInitiationImpl::Factory::Create(
    Context* context) {
  NL_DCHECK(context);
  if (test_factory_) {
    return test_factory_->CreateInstance(context);
  }

  return std::make_unique<NearbyFastInitiationImpl>(context);
}

void NearbyFastInitiationImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyFastInitiationImpl::NearbyFastInitiationImpl(Context* context)
    : context_(context) {
  NL_DCHECK(context);
}

bool NearbyFastInitiationImpl::IsLowEnergySupported() {
  return context_->GetBluetoothAdapter().IsLowEnergySupported();
}

bool NearbyFastInitiationImpl::IsScanOffloadSupported() {
  return context_->GetBluetoothAdapter().IsScanOffloadSupported();
}

bool NearbyFastInitiationImpl::IsAdvertisementOffloadSupported() {
  return context_->GetBluetoothAdapter().IsAdvertisementOffloadSupported();
}

bool NearbyFastInitiationImpl::IsScanning() const {
  return context_->GetFastInitiationManager().IsScanning();
}

bool NearbyFastInitiationImpl::IsAdvertising() const {
  return context_->GetFastInitiationManager().IsAdvertising();
}

void NearbyFastInitiationImpl::StartScanning(
    std::function<void()> devices_discovered_callback,
    std::function<void()> devices_not_discovered_callback,
    std::function<void()> error_callback) {
  if (IsScanning()) {
    NL_LOG(WARNING) << __func__
                    << ": FastInit BLE scanning was started already.";
    error_callback();
    return;
  }

  context_->GetFastInitiationManager().StartScanning(
      std::move(devices_discovered_callback),
      std::move(devices_not_discovered_callback),
      [&, error_callback =
              std::move(error_callback)](FastInitiationManager::Error error) {
        ScanningErrorCodeCallbackHandler(error);
        error_callback();
      });
}

void NearbyFastInitiationImpl::StopScanning(std::function<void()> callback) {
  if (!IsScanning()) {
    NL_LOG(WARNING) << __func__ << ": FastInit BLE scanning is not running.";
    callback();
    return;
  }
  context_->GetFastInitiationManager().StopScanning(
      [&, callback = std::move(callback)]() { callback(); });
}

void NearbyFastInitiationImpl::StartAdvertising(
    FastInitType type, std::function<void()> callback,
    std::function<void()> error_callback) {
  if (IsAdvertising()) {
    NL_LOG(WARNING) << __func__
                    << ": FastInit BLE advertising was started already.";
    error_callback();
    return;
  }

  context_->GetFastInitiationManager().StartAdvertising(
      ::nearby::api::FastInitBleBeacon::FastInitType(type),
      [&, callback = std::move(callback)]() { callback(); },
      [&, error_callback =
              std::move(error_callback)](FastInitiationManager::Error error) {
        AdvertisingErrorCodeCallbackHandler(error);
        error_callback();
      });
}

void NearbyFastInitiationImpl::StopAdvertising(std::function<void()> callback) {
  if (!IsAdvertising()) {
    NL_LOG(WARNING) << __func__ << ": FastInit BLE advertising is not running.";
    callback();
    return;
  }

  context_->GetFastInitiationManager().StopAdvertising(
      [&, callback = std::move(callback)]() { callback(); });
}

void NearbyFastInitiationImpl::ScanningErrorCodeCallbackHandler(
    FastInitiationManager::Error error) {
  switch (error) {
    case FastInitiationManager::Error::kBluetoothRadioUnavailable:
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->HardwareErrorReported(this);
        }
      }
      NL_LOG(ERROR) << __func__
                    << ": FastInit BLE scanning failed due to bluetooth radio "
                       "unavailable.";
      break;
    case FastInitiationManager::Error::kResourceInUse:
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->HardwareErrorReported(this);
        }
      }
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE scanning failed due to bluetooth resources are "
             "in use/at full capacity.";
      break;
    case FastInitiationManager::Error::kDisabledByPolicy:
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE scanning failed due to being disabled by policy.";
      break;
    case FastInitiationManager::Error::kDisabledByUser:
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE scanning failed due to being disabled by user.";
      break;
    case FastInitiationManager::Error::kHardwareNotSupported:
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->HardwareErrorReported(this);
        }
      }
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE scanning failed due to hardware not supported.";
      break;
    case FastInitiationManager::Error::kTransportNotSupported:
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->HardwareErrorReported(this);
        }
      }
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE scanning failed due to transport not supported.";
      break;
    case FastInitiationManager::Error::kConsentRequired:
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE scanning failed due to consent required.";
      break;
    case FastInitiationManager::Error::kUnknown:
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->HardwareErrorReported(this);
        }
      }
      NL_LOG(ERROR) << __func__
                    << ": FastInit BLE scanning failed due to unknown reasons.";
      break;
    default:
      break;
  }
}

void NearbyFastInitiationImpl::AdvertisingErrorCodeCallbackHandler(
    FastInitiationManager::Error error) {
  switch (error) {
    case FastInitiationManager::Error::kBluetoothRadioUnavailable:
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->HardwareErrorReported(this);
        }
      }
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE advertising failed due to bluetooth radio "
             "unavailable.";
      break;
    case FastInitiationManager::Error::kResourceInUse:
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->HardwareErrorReported(this);
        }
      }
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE advertising failed due to bluetooth resources are "
             "in use/at full capacity.";
      break;
    case FastInitiationManager::Error::kDisabledByPolicy:
      NL_LOG(ERROR) << __func__
                    << ": FastInit BLE advertising failed due to being "
                       "disabled by policy.";
      break;
    case FastInitiationManager::Error::kDisabledByUser:
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE advertising failed due to being disabled by user.";
      break;
    case FastInitiationManager::Error::kHardwareNotSupported:
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->HardwareErrorReported(this);
        }
      }
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE advertising failed due to hardware not supported.";
      break;
    case FastInitiationManager::Error::kTransportNotSupported:
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->HardwareErrorReported(this);
        }
      }
      NL_LOG(ERROR) << __func__
                    << ": FastInit BLE advertising failed due to transport not "
                       "supported.";
      break;
    case FastInitiationManager::Error::kConsentRequired:
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE advertising failed due to consent required.";
      break;
    case FastInitiationManager::Error::kUnknown:
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->HardwareErrorReported(this);
        }
      }
      NL_LOG(ERROR)
          << __func__
          << ": FastInit BLE advertising failed due to unknown reasons.";
      break;
    default:
      break;
  }
}

void NearbyFastInitiationImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
  NL_LOG(INFO) << __func__ << ": Fast Initiation observer added.";
}
void NearbyFastInitiationImpl::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
  NL_LOG(INFO) << __func__ << ": Fast Initiation observer removed.";
}
bool NearbyFastInitiationImpl::HasObserver(Observer* observer) {
  return observer_list_.HasObserver(observer);
}

}  // namespace sharing
}  // namespace nearby
