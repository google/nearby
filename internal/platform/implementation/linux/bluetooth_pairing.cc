#include <systemd/sd-bus.h>
#include <utility>

#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_pairing.h"
#include "internal/platform/implementation/linux/bluez.h"

namespace nearby {
namespace linux {

int pairing_reply_handler(sd_bus_message *m, void *userdata,
                          sd_bus_error *err) {
  auto pairing_cb = static_cast<api::BluetoothPairingCallback *>(userdata);
  if (sd_bus_message_is_method_error(m, nullptr)) {
    if (sd_bus_message_is_method_error(
            m, "org.bluez.Error.AuthenticationCanceled")) {
      pairing_cb->on_pairing_error_cb(
          api::BluetoothPairingCallback::PairingError::kAuthCanceled);
    } else if (sd_bus_message_is_method_error(
                   m, "org.bluez.Error.AuthenticationFailed")) {
      pairing_cb->on_pairing_error_cb(
          api::BluetoothPairingCallback::PairingError::kAuthFailed);
    } else if (sd_bus_message_is_method_error(
                   m, "org.bluez.Error.AuthenticationRejected")) {
      pairing_cb->on_pairing_error_cb(
          api::BluetoothPairingCallback::PairingError::kAuthRejected);
    } else if (sd_bus_message_is_method_error(
                   m, "org.bluez.Error.AuthenticationTimeout")) {
      pairing_cb->on_pairing_error_cb(
          api::BluetoothPairingCallback::PairingError::kAuthTimeout);
    } else {
      pairing_cb->on_pairing_error_cb(
          api::BluetoothPairingCallback::PairingError::kAuthFailed);
    }
    return 0;
  }
  if (err) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Error pairing with device: " << err->message;
    pairing_cb->on_pairing_error_cb(
        api::BluetoothPairingCallback::PairingError::kUnknown);
  } else {
    pairing_cb->on_paired_cb();
  }
  return 0;
}

bool BluetoothPairing::InitiatePairing(
    api::BluetoothPairingCallback pairing_cb) {
  if (!system_bus_)
    return false;

  pairing_cb_ = std::move(pairing_cb);

  if (sd_bus_call_method_async(
          system_bus_, nullptr, BLUEZ_SERVICE, device_object_path_.c_str(),
          BLUEZ_DEVICE_INTERFACE, "Pair", &pairing_reply_handler, &pairing_cb_,
          nullptr) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error calling method Pair on device "
                       << device_object_path_;
    return false;
  }
  pairing_cb.on_pairing_initiated_cb(api::PairingParams{
      api::PairingParams::PairingType::kConsent, std::string()});
  return true;
}

bool BluetoothPairing::FinishPairing(
    std::optional<absl::string_view> pin_code) {
  return true;
}

bool BluetoothPairing::CancelPairing() {
  if (!system_bus_)
    return false;

  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  if (sd_bus_call_method(system_bus_, BLUEZ_SERVICE,
                         device_object_path_.c_str(), BLUEZ_DEVICE_INTERFACE,
                         "CancelPairing", &err, nullptr, nullptr)) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Error calling method CancelPairing on device "
                       << device_object_path_ << ": " << err.message;
    return false;
  }
  return true;
}

bool BluetoothPairing::Unpair() {
  if (!system_bus_)
    return false;

  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  if (sd_bus_call_method(system_bus_, BLUEZ_SERVICE, "/org/bluez/hci0",
                         BLUEZ_ADAPTER_INTERFACE, "RemoveDevice", &err, nullptr,
                         "o", device_object_path_.c_str())) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Error calling method CancelPairing on device "
                       << device_object_path_ << ": " << err.message;
    return false;
  }
  return true;
}

bool BluetoothPairing::IsPaired() {
  if (!system_bus_)
    return false;

  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  int paired = 0;
  if (sd_bus_get_property_trivial(
          system_bus_, BLUEZ_SERVICE, device_object_path_.c_str(),
          BLUEZ_DEVICE_INTERFACE, "Bonded", &err, 'b', &paired) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Error getting Bonded property for device "
                       << device_object_path_ << ": " << err.message;
  }
  return paired;
}
} // namespace linux
} // namespace nearby
