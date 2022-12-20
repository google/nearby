// Copyright 2021 Google LLC.
#ifndef NEARBY_PLATFORM_AUDIO_H
#define NEARBY_PLATFORM_AUDIO_H

#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NEARBY_PLATFORM_CONNECTION_STATE_NO_CONNECTION 0
#define NEARBY_PLATFORM_CONNECTION_STATE_PAGING 1
#define NEARBY_PLATFORM_CONNECTION_STATE_NO_DATA_TRANFER 2
#define NEARBY_PLATFORM_CONNECTION_STATE_NON_AUDIO_DATA_TRANSFER 3
#define NEARBY_PLATFORM_CONNECTION_STATE_A2DP 4
#define NEARBY_PLATFORM_CONNECTION_STATE_A2DP_WITH_AVRCP 5
#define NEARBY_PLATFORM_CONNECTION_STATE_HFP 6
#define NEARBY_PLATFORM_CONNECTION_STATE_LE_MEDIA_WITHOUT_CONTROL 7
#define NEARBY_PLATFORM_CONNECTION_STATE_LE_MEDIA_WITH_CONTROL 8
#define NEARBY_PLATFORM_CONNECTION_STATE_LE_CALL 9
#define NEARBY_PLATFORM_CONNECTION_STATE_LE_BIS 10
#define NEARBY_PLATFORM_CONNECTION_STATE_DISABLED_CONNECTION_SWITCH 15

// Multipoint switching preference flag bits. Every bit represents "new profile
// request" vs "current profile request". 1 - preference for switching, 0 -
// preference for not switching
#define NEARBY_SASS_SWITCHING_PREFERENCE_A2DP_VS_A2DP_MASK (1 << 7)
#define NEARBY_SASS_SWITCHING_PREFERENCE_HFP_VS_HFP_MASK (1 << 6)
#define NEARBY_SASS_SWITCHING_PREFERENCE_A2DP_VS_HFP_MASK (1 << 5)
#define NEARBY_SASS_SWITCHING_PREFERENCE_HFP_VS_A2DP_MASK (1 << 4)

// Flags for SwitchActiveAudioSource
// 1 switch to this device, 0 switch to second connected device
#define NEARBY_SASS_SWITCH_ACTIVE_AUDIO_SOURCE_THIS_DEVICE_MASK (1 << 7)
// Resume playing on switch to device after switching, 0 otherwise. Resuming
// playing means the Provider sends a PLAY notification to the Seeker through
// AVRCP profile. If the previous state (before switched away) was not PLAY, the
// Provider should ignore this flag.
#define NEARBY_SASS_SWITCH_ACTIVE_AUDIO_SOURCE_RESUME_MASK (1 << 6)
// 1 reject SCO on switched away device, 0 otherwise
#define NEARBY_SASS_SWITCH_ACTIVE_AUDIO_SOURCE_REJECT_SCO_MASK (1 << 5)
// 1 disconnect Bluetooth on switch away device, 0 otherwise.
#define NEARBY_SASS_SWITCH_ACTIVE_AUDIO_SOURCE_DISCONNECT_PREVIOUS_MASK (1 << 4)

// Flags for SwitchBackAudioSource. Note that the flags below are not bitmasks.
#define NEARBY_SASS_SWITCH_BACK 0x01
#define NEARBY_SASS_SWITCH_BACK_AND_RESUME 0x02

// Flags for NotifySassInitiatedConnection. Note that the flags below are not
// bitmasks.
#define NEARBY_SASS_CONNECTION_INITIATED_BY_SASS 0
#define NEARBY_SASS_CONNECTION_NOT_INITIATED_BY_SASS 1

// Reasons for |on_multipoint_switch_event|
#define NEARBY_SASS_MULTIPOINT_SWITCH_REASON_UNKNOWN 0
#define NEARBY_SASS_MULTIPOINT_SWITCH_REASON_A2DP 1
#define NEARBY_SASS_MULTIPOINT_SWITCH_REASON_HFP 2

// Returns true if right earbud is active
bool nearby_platform_GetEarbudRightStatus();

// Returns true if left earbud is active
bool nearby_platform_GetEarbudLeftStatus();

typedef struct {
  // The platform HAL should call this callback on audio state changes. That is,
  // when any of the query methods would return a new value.
  void (*on_state_change)();
  // This event should be called after a multipoint audio source switch. To make
  // users aware of a multipoint-switch event taking place, SASS Seekers may
  // show a notification to users. So the Provider should notify connected SASS
  // Seekers about the switching event.
  // |reason| is one of NEARBY_SASS_MULTIPOINT_SWITCH_REASON_* constants
  // |peer_address| is the address the new audio source
  // |name| is utf-8 encoded name of the new audio source or NULL if name is not
  // known.
  void (*on_multipoint_switch_event)(uint8_t reason, uint64_t peer_address,
                                     const char* name);
} nearby_platform_AudioCallbacks;

// Returns one of NEARBY_PLATFORM_CONNECTION_STATE_* values
// Call |nearby_platform_AudioCallbacks::on_state_change| when this state
// changes.
unsigned int nearby_platform_GetAudioConnectionState();

// Returns true if the device is on head (or in ear).
// Call |nearby_platform_AudioCallbacks::on_state_change| when on-head state
// changes.
bool nearby_platform_OnHead();

// Returns true if the device can accept another audio connection without
// dropping any of the existing connections.
// Call |nearby_platform_AudioCallbacks::on_state_change| when this state
// changes.
bool nearby_platform_CanAcceptConnection();

// When the device is in focus mode, connection switching is not allowed
// Call |nearby_platform_AudioCallbacks::on_state_change| when this state
// changes.
bool nearby_platform_InFocusMode();

// Returns true if the current connection is auto-recconnected, meaning it is
// not connected by the user. For multi-point connections, returns true if any
// of the existing connections is auto-reconnected.
// Call |nearby_platform_AudioCallbacks::on_state_change| when this state
// changes.
bool nearby_platform_AutoReconnected();

// Sets a bit in the |bitmap| for every connected peer. The bit stays cleared
// for bonded but not connected peers. The order change is acceptable if it is
// unavoidable, e.g. when users factory reset the headset or when the bonded
// device count reaches the upper limit.
// |length| is the |bitmap| length on input in bytes and used space on output.
// For example, if there are 5 bonded devices, then |length| should be set to 1.
// Call |nearby_platform_AudioCallbacks::on_state_change| when this state
// changes.
void nearby_platform_GetConnectionBitmap(uint8_t* bitmap, size_t* length);

// Returns true is SASS state in On
bool nearby_platform_IsSassOn();

// Returns true if the device supports multipoint and it can be switched between
// on and off
bool nearby_platform_IsMultipointConfigurable();

// Returns true is multipoint in On
bool nearby_platform_IsMultipointOn();

// Returns true if the device supports OHD (even if it's turned off at the
// moment)
bool nearby_platform_IsOnHeadDetectionSupported();

// Returns true if OHD is supported and enabled
bool nearby_platform_IsOnHeadDetectionEnabled();

// Enables or disables multipoint
// When |enable| is false, the device should keep the connection with
// |peer_address| and disconnect other connections (if any)
nearby_platform_status nearby_platform_SetMultipoint(uint64_t peer_address,
                                                     bool enable);

// Sets multipoint switching preference flags
nearby_platform_status nearby_platform_SetSwitchingPreference(uint8_t flags);

// Gets switching preference flags
uint8_t nearby_platform_GetSwitchingPreference();

// Switches active audio source (to a connected device). If the flags indicate a
// switch to |peer_address| device but |peer_address| is already the active
// device, then return kNearbyStatusRedundantAction.
// If the flags indicate a switch to another device, then
// |preferred_audio_source| is the address of the next, preferred audio source.
// |preferred_audio_source| is 0 if Nearby SDK cannot figure out what the next
// audio source should be. It may happen if they are no other connected Seekers.
nearby_platform_status nearby_platform_SwitchActiveAudioSource(
    uint64_t peer_address, uint8_t flags, uint64_t preferred_audio_source);

// Switches back to a disconnected audio source.
// |peer_address| is the address of the seeker who sent this command, *not* the
// address of the audio source that the device should switch to. The device
// should connect to the previous, currently disconnected, audio source and
// disconnect from |peer_address|. Ideally, the device should disconnect from
// |peer_address| after returning from this function. This would give Nearby SDK
// a chance to send an ACK message to the seeker.
nearby_platform_status nearby_platform_SwitchBackAudioSource(
    uint64_t peer_address, uint8_t flags);

// Notifies the platform if the connection was initiated by SASS. SASS Providers
// may need to know if the connection switching is triggered by SASS to have
// different reactions, e.g. disable earcons for SASS events. The Seeker sends a
// message to notify the Provider that this connection was a SASS initiated
// connection.
nearby_platform_status nearby_platform_NotifySassInitiatedConnection(
    uint64_t peer_address, uint8_t flags);

// Sets drop connection target
// On multipoint headphones, if the preferred connection to be dropped is not
// the least recently used one, SASS Seekers can tell the Provider which device
// to be dropped by using the message below.
nearby_platform_status nearby_platform_SetDropConnectionTarget(
    uint64_t peer_address, uint8_t flags);

// Returns the BT address of the active audio source
// Call |nearby_platform_AudioCallbacks::on_state_change| when active audio
// source changes.
uint64_t nearby_platform_GetActiveAudioSource();

// Initializes Audio module
nearby_platform_status nearby_platform_AudioInit(
    const nearby_platform_AudioCallbacks* audio_interface);

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_PLATFORM_AUDIO_H */
