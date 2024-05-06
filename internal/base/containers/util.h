// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_CONTAINERS_UTIL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_CONTAINERS_UTIL_H_

#ifdef NEARBY_CHROMIUM
#error "Use chromium headers when NEARBY_CHROMIUM is defined."
#endif

#include <stdint.h>

namespace base {

// TODO(crbug.com/40565371): What we really need is for checked_math.h to be
// able to do checked arithmetic on pointers.
template <typename T>
inline uintptr_t get_uintptr(const T* t) {
  return reinterpret_cast<uintptr_t>(t);
}

}  // namespace base

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_CONTAINERS_UTIL_H_
