#include "platform/container_of.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location::nearby {

TEST(OffsetOf, OffsetOfTest) {
  struct [[gnu::packed]] S {
    char x;
    double y;
  };
  EXPECT_EQ(OffsetOf(&S::x), 0U);
  EXPECT_EQ(OffsetOf(&S::y), sizeof(S::x));
}

TEST(OffsetOf, ExplicitOffsetOfTest) {
  struct [[gnu::packed]] S1 { int x; };
  struct [[gnu::packed]] S2 { double y; };
  struct [[gnu::packed]] S : public S1, S2 { char t; };
  EXPECT_EQ((OffsetOf<decltype(std::declval<S>().x), S>(&S::x)), 0U);
  EXPECT_EQ((OffsetOf<decltype(std::declval<S>().y), S>(&S::y)), sizeof(S::x));
}

TEST(ContainerOf, ContainerOfTest) {
  struct [[gnu::packed]] S {
    char x;
    double y;
  } s;
  char* p = &s.x;
  double* q = &s.y;
  EXPECT_EQ(ContainerOf(p, &S::x), &s);
  EXPECT_EQ(ContainerOf(q, &S::y), &s);
}

TEST(ContainerOf, ContainerOfTestConst) {
  struct [[gnu::packed]] S {
    char x;
    double y;
  } s;
  const char* p = &s.x;
  const double* q = &s.y;
  EXPECT_EQ(ContainerOf(p, &S::x), &s);
  EXPECT_EQ(ContainerOf(q, &S::y), &s);
}

}  // namespace location::nearby
