// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_NUMERICS_ANGLE_CONVERSIONS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_NUMERICS_ANGLE_CONVERSIONS_H_

#ifdef NEARBY_CHROMIUM
#error "Use chromium headers when NEARBY_CHROMIUM is defined."
#endif

#include <concepts>
#include <numbers>

namespace base {

template <typename T>
  requires std::floating_point<T>
constexpr T DegToRad(T deg) {
  return deg * std::numbers::pi_v<T> / 180;
}

template <typename T>
  requires std::floating_point<T>
constexpr T RadToDeg(T rad) {
  return rad * 180 / std::numbers::pi_v<T>;
}

}  // namespace base

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_NUMERICS_ANGLE_CONVERSIONS_H_
