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

#include "fakes.h"
#include "nearby.h"
#include "nearby_fp_client.h"
#include "nearby_platform_audio.h"

static const nearby_platform_AudioCallbacks* callbacks;

static bool multipoint = false;
static uint8_t switching_preference_flags = 0;
static uint64_t switch_active_peer_address = 0;
static uint8_t switch_active_flags = 0;
static uint64_t switch_active_preferred_audio_source = 0;
static uint64_t switch_back_peer_address = 0;
static uint8_t switch_back_flags = 0;
static uint64_t notify_peer_address = 0;
static uint8_t notify_flags = 0;
static uint64_t drop_peer_address = 0;
static uint8_t drop_flags = 0;
static uint64_t active_peer_address = 0;

bool nearby_platform_GetEarbudRightStatus() { return false; }

bool nearby_platform_GetEarbudLeftStatus() { return false; }

unsigned int nearby_platform_GetAudioConnectionState() {
  return NEARBY_PLATFORM_CONNECTION_STATE_A2DP_WITH_AVRCP;
}

bool nearby_platform_OnHead() { return true; }

// Returns true if the device can accept another audio connection without
// dropping any of the existing connections.
bool nearby_platform_CanAcceptConnection() { return false; }

// When the device is in focus mode, connection switching is not allowed
bool nearby_platform_InFocusMode() { return false; }

// Returns true if the current connection is auto-recconnected, meaning it is
// not connected by the user. For multi-point connections, returns true if any
// of the existing connections is auto-reconnected.
bool nearby_platform_AutoReconnected() { return false; }

// Sets a bit in the |bitmap| for every connected peer. The bit stays cleared
// for bonded but not connected peers. The order change is acceptable if it is
// unavoidable, e.g. when users factory reset the headset or when the bonded
// device count reaches the upper limit.
// |length| is the |bitmap| length on input in bytes and used space on output.
// For example, if there are 5 bonded devices, then |length| should be set to 1.
void nearby_platform_GetConnectionBitmap(uint8_t* bitmap, size_t* length) {
  if (*length > 0) {
    bitmap[0] = 0x09;
    *length = 1;
  }
}

bool nearby_platform_IsSassOn() { return true; }

bool nearby_platform_IsMultipointConfigurable() { return true; }

bool nearby_platform_IsMultipointOn() { return multipoint; }

bool nearby_platform_IsOnHeadDetectionSupported() { return true; }

bool nearby_platform_IsOnHeadDetectionEnabled() { return false; }

nearby_platform_status nearby_platform_SetMultipoint(uint64_t peer_address,
                                                     bool enable) {
  multipoint = enable;
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_SetSwitchingPreference(uint8_t flags) {
  switching_preference_flags = flags;
  return kNearbyStatusOK;
}

// Gets switching preference flags
uint8_t nearby_platform_GetSwitchingPreference() {
  return switching_preference_flags;
}

nearby_platform_status nearby_platform_SwitchActiveAudioSource(
    uint64_t peer_address, uint8_t flags, uint64_t preferred_audio_source) {
  switch_active_peer_address = peer_address;
  switch_active_flags = flags;
  switch_active_preferred_audio_source = preferred_audio_source;
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_SwitchBackAudioSource(
    uint64_t peer_address, uint8_t flags) {
  switch_back_peer_address = peer_address;
  switch_back_flags = flags;
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_NotifySassInitiatedConnection(
    uint64_t peer_address, uint8_t flags) {
  notify_peer_address = peer_address;
  notify_flags = flags;
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_SetDropConnectionTarget(
    uint64_t peer_address, uint8_t flags) {
  drop_peer_address = peer_address;
  drop_flags = flags;
  return kNearbyStatusOK;
}

uint64_t nearby_platform_GetActiveAudioSource() { return active_peer_address; }

void nearby_test_fakes_SetActiveAudioSource(uint64_t peer_address) {
    active_peer_address = peer_address;
}

// Initializes Audio module
nearby_platform_status nearby_platform_AudioInit(
    const nearby_platform_AudioCallbacks* audio_interface) {
  multipoint = false;
  switching_preference_flags = 0;
  switch_active_peer_address = 0;
  switch_active_flags = 0;
  switch_active_preferred_audio_source = 0;
  switch_back_peer_address = 0;
  switch_back_flags = 0;
  notify_peer_address = 0;
  notify_flags = 0;
  drop_peer_address = 0;
  drop_flags = 0;

  callbacks = audio_interface;
  return kNearbyStatusOK;
}

uint64_t nearby_test_fakes_GetSassSwitchActiveSourcePeerAddress() {
  return switch_active_peer_address;
}

uint8_t nearby_test_fakes_GetSassSwitchActiveSourceFlags() {
  return switch_active_flags;
}

uint64_t nearby_test_fakes_GetSassSwitchActiveSourcePreferredAudioSource() {
  return switch_active_preferred_audio_source;
}

uint64_t nearby_test_fakes_GetSassSwitchBackPeerAddress() {
  return switch_back_peer_address;
}

uint8_t nearby_test_fakes_GetSassSwitchBackFlags() { return switch_back_flags; }

uint64_t nearby_test_fakes_GetSassNotifyInitiatedConnectionPeerAddress() {
  return notify_peer_address;
}

uint8_t nearby_test_fakes_GetSassNotifyInitiatedConnectionFlags() {
  return notify_flags;
}

uint64_t nearby_test_fakes_GetSassDropConnectionTargetPeerAddress() {
  return drop_peer_address;
}

uint8_t nearby_test_fakes_GetSassDropConnectionTargetFlags() {
  return drop_flags;
}

void nearby_test_fakes_SassMultipointSwitch(uint8_t reason,
                                            uint64_t peer_address,
                                            const char* name) {
  callbacks->on_multipoint_switch_event(reason, peer_address, name);
}
