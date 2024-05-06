// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_NUMERICS_RANGES_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_NUMERICS_RANGES_H_

#ifdef NEARBY_CHROMIUM
#error "Use chromium headers when NEARBY_CHROMIUM is defined."
#endif

#include <cmath>
#include <type_traits>

namespace base {

template <typename T>
constexpr bool IsApproximatelyEqual(T lhs, T rhs, T tolerance) {
  static_assert(std::is_arithmetic_v<T>, "Argument must be arithmetic");
  return std::abs(rhs - lhs) <= tolerance;
}

}  // namespace base

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_NUMERICS_RANGES_H_
