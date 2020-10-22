#ifndef PLATFORM_BASE_TYPES_H_
#define PLATFORM_BASE_TYPES_H_

#include <type_traits>

namespace location {
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
}  // namespace location

#endif  // PLATFORM_BASE_TYPES_H_
