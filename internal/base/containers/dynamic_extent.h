// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_CONTAINERS_DYNAMIC_EXTENT_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_CONTAINERS_DYNAMIC_EXTENT_H_

#ifdef NEARBY_CHROMIUM
#error "Use chromium headers when NEARBY_CHROMIUM is defined."
#endif

#include <cstddef>
#include <limits>

namespace base {

// [views.constants]
inline constexpr size_t dynamic_extent = std::numeric_limits<size_t>::max();

}  // namespace base

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_CONTAINERS_DYNAMIC_EXTENT_H_
