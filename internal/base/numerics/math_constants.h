// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_NUMERICS_MATH_CONSTANTS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_NUMERICS_MATH_CONSTANTS_H_

#ifdef NEARBY_CHROMIUM
#error "Use chromium headers when NEARBY_CHROMIUM is defined."
#endif

namespace base {
// The mean acceleration due to gravity on Earth in m/s^2.
constexpr double kMeanGravityDouble = 9.80665;
constexpr float kMeanGravityFloat = 9.80665f;

}  // namespace base

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_NUMERICS_MATH_CONSTANTS_H_
