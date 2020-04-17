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

#ifndef PLATFORM_CONTAINER_OF_H_
#define PLATFORM_CONTAINER_OF_H_

#include <cstddef>
#include <type_traits>

namespace location::nearby {

// Similar to offsetof() macro, but implemented in a type-safe way,
// OffsetOf(<pointer-to-member>) returns the byte offset of a given data
// member in the ClassType.
// Behavior is undefined if member is not a direct, non-static data member of
// type ClassType.
// usage example:
// struct S { int x; double y; };
// size_t y_offset = OffsetOf(&S::y);
// CHECK(y_offset >= sizeof(int));
//
// the following is not guaranteed to work:
// struct S1 { int x; };
// struct S2 { double y; };
// struct S : public S1, S2 { char t; };
// size_t y_offset_bad = OffsetOf(&S::y);
// because S::y is not a direct member of S; it is a member by inheritance.
// To make sure OffsetOf works with inherited members, it must be called
// with explicitly defined template parameters, as follows:
// size_t y_offset_ok = OffsetOf<double,S>(&S::y);
//
// However, the following is guaranteed to work:
// struct S1 { int x; };
// struct S2 { double y; };
// struct S3 { double z; };
// struct S : public S1, S2 { S3 s3; char t; };
// size_t s3_offset = OffsetOf(&S::s3);

template <typename ValueType, typename ClassType>
constexpr size_t OffsetOf(const ValueType ClassType::*member) {
  std::aligned_storage_t<sizeof(ClassType), alignof(ClassType)> obj_memory;
  ClassType* obj = reinterpret_cast<ClassType*>(&obj_memory);
  return reinterpret_cast<size_t>(&(obj->*member)) -
         reinterpret_cast<size_t>(obj);
}

// Similar to Linux containerof() macro, this function returns pointer to
// the type instance that contains the specified member;
// ContainerOf(<pointer to variable instance of type ValueType, which is member
// of ClassType>, <pointer-to-ClassType-member>);
// usage example:
// struct S { int x; double y; } a;
// S *b = ContainerOf(&a.y, &S::y);
// CHECK(b == &a);
template <typename ValueType, typename ClassType>
ClassType* ContainerOf(ValueType* ptr, ValueType ClassType::*member) {
  using BaseValueType = std::remove_volatile_t<ValueType>;
  return reinterpret_cast<ClassType*>(
      reinterpret_cast<char*>(const_cast<BaseValueType*>(ptr)) -
      OffsetOf(member));
}

template <typename ValueType, typename ClassType>
const ClassType* ContainerOf(const ValueType* ptr,
                             ValueType ClassType::*member) {
  using BaseValueType = std::remove_volatile_t<ValueType>;
  return reinterpret_cast<const ClassType*>(
      reinterpret_cast<const char*>(const_cast<BaseValueType*>(ptr)) -
      OffsetOf(member));
}

}  // namespace location::nearby

#endif  // PLATFORM_CONTAINER_OF_H_
