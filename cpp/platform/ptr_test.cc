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

TEST(PtrTest, RefCountedPtr_IsRefCounted_Works) {
  Ptr<int> ref_counted = MakeRefCountedPtr(new int(1234));
  Ptr<int> manually_counted = MakePtr(new int(1234));
  ScopedPtr<Ptr<int> > scoped_manually_counted(manually_counted);

  ASSERT_TRUE(ref_counted.isRefCounted());
  ASSERT_FALSE(manually_counted.isRefCounted());
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

TEST(PtrTest, AssignmentOperator_ManuallyCountedToManuallyCounted) {
  Ptr<int> manually_counted_1 = MakePtr(new int(1234));
  Ptr<int> manually_counted_2 = MakePtr(new int(5678));
  // Avoid leaks.
  ScopedPtr<Ptr<int> > scoped_manually_counted_1(manually_counted_1);
  ScopedPtr<Ptr<int> > scoped_manually_counted_2(manually_counted_2);

  manually_counted_2 = manually_counted_1;

  ASSERT_EQ(1234, *manually_counted_1);
  ASSERT_EQ(1234, *manually_counted_2);
  ASSERT_EQ(1234, *scoped_manually_counted_1);
  ASSERT_EQ(5678, *scoped_manually_counted_2);
}

TEST(PtrTest, AssignmentOperator_RefCountedToManuallyCounted) {
  Ptr<int> ref_counted = MakeRefCountedPtr(new int(1234));
  Ptr<int> manually_counted = MakePtr(new int(5678));
  // Avoid leaks.
  ScopedPtr<Ptr<int> > scoped_manually_counted(manually_counted);

  manually_counted = ref_counted;

  ASSERT_EQ(1234, *ref_counted);
  ASSERT_EQ(1234, *manually_counted);
  ASSERT_EQ(5678, *scoped_manually_counted);
}

TEST(PtrTest, AssignmentOperator_ManuallyCountedToRefCounted) {
  Ptr<int> manually_counted = MakePtr(new int(1234));
  Ptr<int> ref_counted = MakeRefCountedPtr(new int(5678));
  // Avoid leaks.
  ScopedPtr<Ptr<int> > scoped_manually_counted(manually_counted);

  ref_counted = manually_counted;

  ASSERT_EQ(1234, *ref_counted);
  ASSERT_EQ(1234, *manually_counted);
  ASSERT_EQ(1234, *scoped_manually_counted);
}

TEST(PtrTest, AssignmentOperator_SelfAssignment_ManuallyCounted) {
  Ptr<int> manually_counted_1 = MakePtr(new int(1234));
  Ptr<int> manually_counted_2(manually_counted_1);
  // Avoid leaks.
  ScopedPtr<Ptr<int> > scoped_manually_counted_1(manually_counted_1);

  manually_counted_1 = manually_counted_2;

  ASSERT_EQ(1234, *manually_counted_1);
  ASSERT_EQ(1234, *manually_counted_2);
  ASSERT_EQ(1234, *scoped_manually_counted_1);
}

TEST(PtrTest, AssignmentOperator_SelfAssignment_RefCounted) {
  Ptr<int> ref_counted_1 = MakeRefCountedPtr(new int(1234));
  Ptr<int> ref_counted_2(ref_counted_1);

  ref_counted_1 = ref_counted_2;

  ASSERT_EQ(1234, *ref_counted_1);
  ASSERT_EQ(1234, *ref_counted_2);
}

TEST(PtrTest, EqualityOperator_ManuallyCounted) {
  Ptr<int> manually_counted_1 = MakePtr(new int(1234));
  Ptr<int> manually_counted_2(manually_counted_1);
  // Avoid leaks.
  ScopedPtr<Ptr<int> > scoped_manually_counted_1(manually_counted_1);

  ASSERT_TRUE(manually_counted_1 == manually_counted_2);

  manually_counted_1 = manually_counted_2;

  ASSERT_TRUE(manually_counted_1 == manually_counted_2);
}

TEST(PtrTest, EqualityOperator_RefCounted) {
  Ptr<int> ref_counted_1 = MakeRefCountedPtr(new int(1234));
  Ptr<int> ref_counted_2(ref_counted_1);

  ASSERT_TRUE(ref_counted_1 == ref_counted_2);

  ref_counted_1 = ref_counted_2;

  ASSERT_TRUE(ref_counted_1 == ref_counted_2);
}

TEST(PtrTest, EqualityOperator_ManuallyAndRefCounted) {
  int* raw = new int(1234);
  Ptr<int> manually_counted = MakePtr(raw);
  Ptr<int> ref_counted = MakeRefCountedPtr(raw);
  // No need for a ScopedPtr for manually_counted here because we know that
  // ref_counted will take care of deallocating 'raw'.

  ASSERT_FALSE(manually_counted == ref_counted);
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

TEST(PtrTest, DerivedToBaseConversion_ManuallyCounted) {
  Ptr<Derived> derived = MakePtr(new Derived(1234));
  Ptr<Base> base = derived;
  // Avoid leaks.
  ScopedPtr<Ptr<Derived> > scoped_derived(derived);

  ASSERT_EQ(1234, base->getInt());
  ASSERT_EQ(1234, derived->getInt());
  ASSERT_EQ(1234, scoped_derived->getInt());
}

TEST(PtrTest, DerivedToBaseConversion_RefCounted) {
  Ptr<Derived> derived = MakeRefCountedPtr(new Derived(1234));
  Ptr<Base> base = derived;

  ASSERT_EQ(1234, derived->getInt());
  derived.destroy();
  // Additionally, make sure that 'base' is valid even after 'derived' has been
  // destroyed.
  ASSERT_EQ(1234, base->getInt());
}

TEST(PtrTest, ScopedPtr_Release_ManuallyCounted) {
  Ptr<int> manually_counted_1 = MakePtr(new int(1234));
  ScopedPtr<Ptr<int> > scoped_manually_counted_1(manually_counted_1);

  Ptr<int> manually_counted_2 = scoped_manually_counted_1.release();
  // Avoid leaks.
  ScopedPtr<Ptr<int> > scoped_manually_counted_2(manually_counted_2);

  ASSERT_TRUE(scoped_manually_counted_1.isNull());
  ASSERT_EQ(1234, *manually_counted_2);
  ASSERT_EQ(1234, *scoped_manually_counted_2);
}

TEST(PtrTest, ScopedPtr_Release_RefCounted) {
  Ptr<int> ref_counted_1 = MakeRefCountedPtr(new int(1234));
  ScopedPtr<Ptr<int> > scoped_ref_counted_1(ref_counted_1);

  Ptr<int> ref_counted_2 = scoped_ref_counted_1.release();

  ASSERT_TRUE(scoped_ref_counted_1.isNull());
  ASSERT_EQ(1234, *ref_counted_2);
}

TEST(PtrTest, ConstifyPtr_ManuallyCounted) {
  Ptr<int> manually_counted = MakePtr(new int(1234));
  // Avoid leaks.
  ScopedPtr<Ptr<int> > scoped_manually_counted(manually_counted);

  ConstPtr<int> const_manually_counted = ConstifyPtr(manually_counted);

  ASSERT_EQ(1234, *const_manually_counted);
  ASSERT_EQ(1234, *manually_counted);
  ASSERT_EQ(1234, *scoped_manually_counted);
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

TEST(PtrTest, DowncastPtr_ManuallyCounted) {
  Ptr<Derived> derived = MakePtr(new Derived(1234));
  Ptr<Base> base = derived;
  // Avoid leaks.
  ScopedPtr<Ptr<Derived> > scoped_derived(derived);

  Ptr<Derived> derived_from_downcast = DowncastPtr<Derived>(base);

  ASSERT_EQ(1234, base->getInt());
  ASSERT_EQ(1234, derived->getInt());
  ASSERT_EQ(1234, derived_from_downcast->getInt());
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
