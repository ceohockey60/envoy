#include "envoy/common/exception.h"

#include "common/stream_info/filter_state_impl.h"

#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace StreamInfo {
namespace {

class TestStoredTypeTracking : public FilterState::Object {
public:
  TestStoredTypeTracking(int value, size_t* access_count, size_t* destruction_count)
      : value_(value), access_count_(access_count), destruction_count_(destruction_count) {}
  ~TestStoredTypeTracking() {
    if (destruction_count_) {
      ++*destruction_count_;
    }
  }

  int access() const {
    if (access_count_) {
      ++*access_count_;
    }
    return value_;
  }

private:
  int value_;
  size_t* access_count_;
  size_t* destruction_count_;
};

class SimpleType : public FilterState::Object {
public:
  SimpleType(int value) : value_(value) {}

  int access() const { return value_; }

private:
  int value_;
};

class FilterStateImplTest : public testing::Test {
public:
  FilterStateImplTest() { resetFilterState(); }

  void resetFilterState() { filter_state_ = std::make_unique<FilterStateImpl>(); }
  FilterState& filter_state() { return *filter_state_; }

private:
  std::unique_ptr<FilterStateImpl> filter_state_;
};

} // namespace

TEST_F(FilterStateImplTest, Simple) {
  size_t access_count = 0u;
  size_t destruction_count = 0u;
  filter_state().setData(
      "test_name", std::make_unique<TestStoredTypeTracking>(5, &access_count, &destruction_count));
  EXPECT_EQ(0u, access_count);
  EXPECT_EQ(0u, destruction_count);

  EXPECT_EQ(5, filter_state().getData<TestStoredTypeTracking>("test_name").access());
  EXPECT_EQ(1u, access_count);
  EXPECT_EQ(0u, destruction_count);

  resetFilterState();
  EXPECT_EQ(1u, access_count);
  EXPECT_EQ(1u, destruction_count);
}

TEST_F(FilterStateImplTest, SameTypes) {
  size_t access_count_1 = 0u;
  size_t access_count_2 = 0u;
  size_t destruction_count = 0u;
  static const int ValueOne = 5;
  static const int ValueTwo = 6;

  filter_state().setData("test_1", std::make_unique<TestStoredTypeTracking>(
                                       ValueOne, &access_count_1, &destruction_count));
  filter_state().setData("test_2", std::make_unique<TestStoredTypeTracking>(
                                       ValueTwo, &access_count_2, &destruction_count));
  EXPECT_EQ(0u, access_count_1);
  EXPECT_EQ(0u, access_count_2);
  EXPECT_EQ(0u, destruction_count);

  EXPECT_EQ(ValueOne, filter_state().getData<TestStoredTypeTracking>("test_1").access());
  EXPECT_EQ(1u, access_count_1);
  EXPECT_EQ(0u, access_count_2);
  EXPECT_EQ(ValueTwo, filter_state().getData<TestStoredTypeTracking>("test_2").access());
  EXPECT_EQ(1u, access_count_1);
  EXPECT_EQ(1u, access_count_2);
  resetFilterState();
  EXPECT_EQ(2u, destruction_count);
}

TEST_F(FilterStateImplTest, SimpleType) {
  filter_state().setData("test_1", std::make_unique<SimpleType>(1));
  filter_state().setData("test_2", std::make_unique<SimpleType>(2));

  EXPECT_EQ(1, filter_state().getData<SimpleType>("test_1").access());
  EXPECT_EQ(2, filter_state().getData<SimpleType>("test_2").access());
}

TEST_F(FilterStateImplTest, NameConflict) {
  filter_state().setData("test_1", std::make_unique<SimpleType>(1));
  EXPECT_THROW_WITH_MESSAGE(filter_state().setData("test_1", std::make_unique<SimpleType>(2)),
                            EnvoyException, "FilterState::setData<T> called twice with same name.");
  EXPECT_EQ(1, filter_state().getData<SimpleType>("test_1").access());
}

TEST_F(FilterStateImplTest, NameConflictDifferentTypes) {
  filter_state().setData("test_1", std::make_unique<SimpleType>(1));
  EXPECT_THROW_WITH_MESSAGE(
      filter_state().setData("test_1",
                             std::make_unique<TestStoredTypeTracking>(2, nullptr, nullptr)),
      EnvoyException, "FilterState::setData<T> called twice with same name.");
}

TEST_F(FilterStateImplTest, UnknownName) {
  EXPECT_THROW_WITH_MESSAGE(filter_state().getData<SimpleType>("test_1"), EnvoyException,
                            "FilterState::getData<T> called for unknown data name.");
}

TEST_F(FilterStateImplTest, WrongTypeGet) {
  filter_state().setData("test_name",
                         std::make_unique<TestStoredTypeTracking>(5, nullptr, nullptr));
  EXPECT_EQ(5, filter_state().getData<TestStoredTypeTracking>("test_name").access());
  EXPECT_THROW_WITH_MESSAGE(filter_state().getData<SimpleType>("test_name"), EnvoyException,
                            "Data stored under test_name cannot be coerced to specified type");
}

// Add elements to filter state list and simulate a consumer iterating over
// all elements.
TEST_F(FilterStateImplTest, IterateThroughListTillEnd) {
  size_t access_count = 0;
  size_t destruction_count = 0;
  filter_state().addToList<TestStoredTypeTracking>(
      "test_name", std::make_unique<TestStoredTypeTracking>(5, &access_count, &destruction_count));
  filter_state().addToList<TestStoredTypeTracking>(
      "test_name", std::make_unique<TestStoredTypeTracking>(5, &access_count, &destruction_count));
  EXPECT_EQ(0, access_count);
  EXPECT_EQ(0, destruction_count);

  filter_state().forEachListItem<TestStoredTypeTracking>("test_name",
                                                         [&](const TestStoredTypeTracking& t) {
                                                           EXPECT_EQ(5, t.access());
                                                           return true;
                                                         });

  EXPECT_EQ(2u, access_count);
  EXPECT_EQ(0, destruction_count);

  resetFilterState();
  EXPECT_EQ(2u, access_count);
  EXPECT_EQ(2u, destruction_count);
}

// Add elements to filter state list and simulate a consumer iterating over
// elements and breaking out of the loop by returning false.
TEST_F(FilterStateImplTest, IterateThroughListAndBreak) {
  size_t access_count = 0;
  size_t destruction_count = 0;
  filter_state().addToList<TestStoredTypeTracking>(
      "test_name", std::make_unique<TestStoredTypeTracking>(5, &access_count, &destruction_count));
  filter_state().addToList<TestStoredTypeTracking>(
      "test_name", std::make_unique<TestStoredTypeTracking>(5, &access_count, &destruction_count));
  EXPECT_EQ(0, access_count);
  EXPECT_EQ(0, destruction_count);

  filter_state().forEachListItem<TestStoredTypeTracking>("test_name",
                                                         [&](const TestStoredTypeTracking& t) {
                                                           EXPECT_EQ(5, t.access());
                                                           return false;
                                                         });

  EXPECT_EQ(1u, access_count);
  EXPECT_EQ(0, destruction_count);

  resetFilterState();
  EXPECT_EQ(1u, access_count);
  EXPECT_EQ(2u, destruction_count);
}

// Check that list and (unary) data elements have no namespace conflicts by
// adding a list element and a data element with same key.
TEST_F(FilterStateImplTest, NoNameConflictBetweenDataAndList) {
  filter_state().setData("test_1", std::make_unique<SimpleType>(1));
  filter_state().addToList<SimpleType>("test_1", std::make_unique<SimpleType>(2));
  EXPECT_EQ(1, filter_state().getData<SimpleType>("test_1").access());
  filter_state().forEachListItem<SimpleType>("test_1", [&](const SimpleType& t) {
    EXPECT_EQ(2, t.access());
    return true;
  });
}

// Check that adding different types to the same list causes exception.
TEST_F(FilterStateImplTest, ErrorAddingDifferentTypesToSameList) {
  filter_state().addToList<SimpleType>("test_1", std::make_unique<SimpleType>(1));
  EXPECT_THROW_WITH_MESSAGE(
      filter_state().addToList<TestStoredTypeTracking>(
          "test_1", std::make_unique<TestStoredTypeTracking>(2, nullptr, nullptr)),
      EnvoyException, "List test_1 does not conform to the specified type");
}

// Check that adding ForEachListItem throws error when types don't match.
TEST_F(FilterStateImplTest, WrongTypeInForEachListItem) {
  filter_state().addToList<TestStoredTypeTracking>(
      "test_name", std::make_unique<TestStoredTypeTracking>(5, nullptr, nullptr));
  EXPECT_THROW_WITH_MESSAGE(filter_state().forEachListItem<SimpleType>(
                                "test_name", [&](const SimpleType&) { return true; }),
                            EnvoyException,
                            "Element in list test_name cannot be coerced to specified type");
}

namespace {

class A : public FilterState::Object {};

class B : public A {};

class C : public B {};

} // namespace

TEST_F(FilterStateImplTest, FungibleInheritance) {
  filter_state().setData("testB", std::make_unique<B>());
  EXPECT_TRUE(filter_state().hasData<B>("testB"));
  EXPECT_TRUE(filter_state().hasData<A>("testB"));
  EXPECT_FALSE(filter_state().hasData<C>("testB"));

  filter_state().addToList<B>("testB", std::make_unique<B>());
  EXPECT_TRUE(filter_state().hasList<B>("testB"));
  EXPECT_TRUE(filter_state().hasList<A>("testB"));
  EXPECT_FALSE(filter_state().hasList<C>("testB"));

  filter_state().setData("testC", std::make_unique<C>());
  EXPECT_TRUE(filter_state().hasData<B>("testC"));
  EXPECT_TRUE(filter_state().hasData<A>("testC"));
  EXPECT_TRUE(filter_state().hasData<C>("testC"));

  filter_state().addToList<C>("testC", std::make_unique<C>());
  EXPECT_TRUE(filter_state().hasList<B>("testC"));
  EXPECT_TRUE(filter_state().hasList<A>("testC"));
  EXPECT_TRUE(filter_state().hasList<C>("testC"));
}

TEST_F(FilterStateImplTest, HasData) {
  filter_state().setData("test_1", std::make_unique<SimpleType>(1));
  EXPECT_TRUE(filter_state().hasData<SimpleType>("test_1"));
  EXPECT_FALSE(filter_state().hasData<SimpleType>("test_2"));
  EXPECT_FALSE(filter_state().hasData<TestStoredTypeTracking>("test_1"));
  EXPECT_FALSE(filter_state().hasData<TestStoredTypeTracking>("test_2"));
  EXPECT_TRUE(filter_state().hasDataWithName("test_1"));
  EXPECT_FALSE(filter_state().hasDataWithName("test_2"));
}

TEST_F(FilterStateImplTest, HasList) {
  filter_state().addToList<SimpleType>("test_1", std::make_unique<SimpleType>(1));
  EXPECT_TRUE(filter_state().hasList<SimpleType>("test_1"));
  EXPECT_FALSE(filter_state().hasList<SimpleType>("test_2"));
  EXPECT_FALSE(filter_state().hasList<TestStoredTypeTracking>("test_1"));
  EXPECT_FALSE(filter_state().hasList<TestStoredTypeTracking>("test_2"));
}

} // namespace StreamInfo
} // namespace Envoy
