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

#include "platform/ptr.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {

TEST(PtrTest, RefCountedPtr_SingleReference) {
  Ptr<int> ref_counted = MakeRefCountedPtr(new int(1234));

  // We just want to make sure that this test doesn't lead to a leak.
  SUCCEED();
}

TEST(PtrTest, RefCountedPtr_MultipleReferences) {
  Ptr<int> ref_counted_1 = MakeRefCountedPtr(new int(1234));
  Ptr<int> ref_counted_2 = ref_counted_1;
  Ptr<int> ref_counted_3(ref_counted_2);

  // We just want to make sure that this test doesn't lead to a leak, nor to
  // double-deletion.
  SUCCEED();
}

TEST(PtrTest, RefCountedPtr_MultipleReferencesWithScoped) {
  Ptr<int> ref_counted = MakeRefCountedPtr(new int(1234));
  ScopedPtr<Ptr<int> > scoped_ref_counted_1(ref_counted);
  ScopedPtr<Ptr<int> > scoped_ref_counted_2(ref_counted);

  // We just want to make sure that this test doesn't lead to a leak, nor to
  // double-deletion.
  SUCCEED();
}

TEST(PtrTest, AssignmentOperator_RefCountedToRefCounted) {
  Ptr<int> ref_counted_1 = MakeRefCountedPtr(new int(1234));
  Ptr<int> ref_counted_2 = MakeRefCountedPtr(new int(5678));

  ref_counted_2 = ref_counted_1;

  ASSERT_EQ(1234, *ref_counted_1);
  ASSERT_EQ(1234, *ref_counted_2);
}

TEST(PtrTest, AssignmentOperator_SelfAssignment_RefCounted) {
  Ptr<int> ref_counted_1 = MakeRefCountedPtr(new int(1234));
  Ptr<int> ref_counted_2(ref_counted_1);

  ref_counted_1 = ref_counted_2;

  ASSERT_EQ(1234, *ref_counted_1);
  ASSERT_EQ(1234, *ref_counted_2);
}

TEST(PtrTest, EqualityOperator_RefCounted) {
  Ptr<int> ref_counted_1 = MakeRefCountedPtr(new int(1234));
  Ptr<int> ref_counted_2(ref_counted_1);

  ASSERT_TRUE(ref_counted_1 == ref_counted_2);

  ref_counted_1 = ref_counted_2;

  ASSERT_TRUE(ref_counted_1 == ref_counted_2);
}

namespace {

class Base {
 public:
  virtual ~Base() {}

  virtual int getInt() const = 0;
};

class Derived : public Base {
 public:
  explicit Derived(int i) : i_(i) {}
  ~Derived() override {}

  int getInt() const override { return i_; }

 private:
  const int i_;
};

}  // namespace

TEST(PtrTest, DerivedToBaseConversion_RefCounted) {
  Ptr<Derived> derived = MakeRefCountedPtr(new Derived(1234));
  Ptr<Base> base = derived;

  ASSERT_EQ(1234, derived->getInt());
  derived.destroy();
  // Additionally, make sure that 'base' is valid even after 'derived' has been
  // destroyed.
  ASSERT_EQ(1234, base->getInt());
}

TEST(PtrTest, DistinctValuesAreNotEqual) {
  Ptr<int> value1 = MakePtr(new int(5));
  Ptr<int> value2 = MakePtr(new int(6));

  ASSERT_NE(value1, value2);
}

TEST(PtrTest, SameValuesAreEqual) {
  Ptr<int> value1 = MakePtr(new int(5));
  Ptr<int> value2 = MakePtr(new int(5));

  ASSERT_EQ(value1, value2);
}

TEST(PtrTest, ScopedPtr_Release_RefCounted) {
  Ptr<int> ref_counted_1 = MakeRefCountedPtr(new int(1234));
  ScopedPtr<Ptr<int> > scoped_ref_counted_1(ref_counted_1);

  Ptr<int> ref_counted_2 = scoped_ref_counted_1.release();

  ASSERT_TRUE(scoped_ref_counted_1.isNull());
  ASSERT_EQ(1234, *ref_counted_1);
  ASSERT_EQ(1234, *ref_counted_2);
}

TEST(PtrTest, ScopedPtr_Release_RefCounted_Stay_Valid) {
  Ptr<int> ref_counted_1 = MakeRefCountedPtr(new int(1234));
  Ptr<int> ref_counted_2 = ref_counted_1;
  ScopedPtr<Ptr<int> > scoped_ref_counted_1(ref_counted_1);

  Ptr<int> ref_counted_3 = scoped_ref_counted_1.release();

  ASSERT_TRUE(scoped_ref_counted_1.isNull());
  ASSERT_EQ(1234, *ref_counted_2);
  ASSERT_EQ(1234, *ref_counted_3);
}

TEST(PtrTest, ConstifyPtr_RefCounted) {
  Ptr<int> ref_counted = MakeRefCountedPtr(new int(1234));

  ConstPtr<int> const_ref_counted = ConstifyPtr(ref_counted);

  ASSERT_EQ(1234, *ref_counted);
  ref_counted.destroy();
  // Additionally, make sure that const_ref_counted is valid even after
  // ref_counted has been destroyed.
  ASSERT_EQ(1234, *const_ref_counted);
}

TEST(PtrTest, DowncastPtr_RefCounted) {
  Ptr<Derived> derived = MakeRefCountedPtr(new Derived(1234));
  Ptr<Base> base = derived;

  Ptr<Derived> derived_from_downcast = DowncastPtr<Derived>(base);

  ASSERT_EQ(1234, base->getInt());
  base.destroy();
  ASSERT_EQ(1234, derived->getInt());
  derived.destroy();
  // Additionally, make sure that derived_from_downcast is valid even after
  // derived has been destroyed.
  ASSERT_EQ(1234, derived_from_downcast->getInt());
}

}  // namespace nearby
}  // namespace location
