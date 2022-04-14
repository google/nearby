// Copyright 2021 Google LLC.
#ifndef NEARBY_PLATFORM_AUDIO_H
#define NEARBY_PLATFORM_AUDIO_H

#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Returns true if right earbud is active
bool nearby_platform_GetEarbudRightStatus();

// Returns true if left earbud is active
bool nearby_platform_GetEarbudLeftStatus();

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_PLATFORM_AUDIO_H */
