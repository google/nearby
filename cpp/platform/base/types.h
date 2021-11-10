// Copyright 2020 Google LLC
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

#ifndef PLATFORM_BASE_TYPES_H_
#define PLATFORM_BASE_TYPES_H_

#include <type_traits>

namespace nearby {

// Similar to static_cast, but will assert that Derived is a derived type of
// Base.
// Usage:
//   class A {};
//   class B : public A {};
//   class C {};
//   B b;
//   A* a = &b;
//   B* b2 = down_cast<B*>(a);  // This is OK.
//   C* c = down_cast<C*>(a);   // This will fail to compile.
template <typename Derived, typename Base>
inline Derived down_cast(Base* value) {
  using DerivedType = typename std::remove_pointer<Derived>::type;
  static_assert(std::is_base_of<Base, DerivedType>::value,
                "incompatible casting");
  return static_cast<Derived>(value);
}

}  // namespace nearby

#endif  // PLATFORM_BASE_TYPES_H_
