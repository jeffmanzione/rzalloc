#include "rzalloc/rzalloc.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

struct TestObject {
  int x;
  int y;
};

class RzallocTest : public ::testing::Test {
 protected:
  RzallocArena arena;

  void SetUp() override { ASSERT_TRUE(arena_init(&arena, sizeof(TestObject))); }

  void TearDown() override { arena_clear(&arena); }
};

TEST_F(RzallocTest, InitSetsObjectSizeCorrectly) {
  EXPECT_EQ(arena_object_size(&arena), sizeof(TestObject));
}

TEST_F(RzallocTest, MallocReturnsNonNull) {
  TestObject* obj = (TestObject*)arena_malloc(&arena);
  EXPECT_NE(obj, nullptr);
}

TEST_F(RzallocTest, CallocZeroesMemory) {
  TestObject* obj = (TestObject*)arena_calloc(&arena);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->x, 0);
  EXPECT_EQ(obj->y, 0);
}

TEST_F(RzallocTest, FreeDecrementsCount) {
  TestObject* obj = (TestObject*)arena_malloc(&arena);
  ASSERT_NE(obj, nullptr);

  EXPECT_EQ(arena_current_num_objects(&arena), 1u);

  EXPECT_TRUE(arena_free(&arena, obj));
  EXPECT_EQ(arena_current_num_objects(&arena), 0u);
}

TEST_F(RzallocTest, DoubleFreeReturnsFalse) {
  TestObject* obj = (TestObject*)arena_malloc(&arena);
  ASSERT_NE(obj, nullptr);

  EXPECT_TRUE(arena_free(&arena, obj));
  EXPECT_FALSE(arena_free(&arena, obj));  // second free should fail
}

TEST_F(RzallocTest, ReusesFreedObjects) {
  // Allocate other object first
  arena_malloc(&arena);

  TestObject* b = (TestObject*)arena_malloc(&arena);

  EXPECT_TRUE(arena_free(&arena, b));

  TestObject* c = (TestObject*)arena_malloc(&arena);

  // c should reuse b's slot
  EXPECT_EQ((char*)c, (char*)b);
}

TEST_F(RzallocTest, MultipleFreesReuseInLifoOrder) {
  TestObject* a = (TestObject*)arena_malloc(&arena);
  TestObject* b = (TestObject*)arena_malloc(&arena);
  TestObject* c = (TestObject*)arena_malloc(&arena);

  arena_free(&arena, a);  // free list: a
  arena_free(&arena, b);  // free list: b -> a
  arena_free(&arena, c);  // free list: c -> b -> a

  TestObject* r1 = (TestObject*)arena_malloc(&arena);
  TestObject* r2 = (TestObject*)arena_malloc(&arena);
  TestObject* r3 = (TestObject*)arena_malloc(&arena);

  EXPECT_EQ((char*)r1, (char*)c);
  EXPECT_EQ((char*)r2, (char*)b);
  EXPECT_EQ((char*)r3, (char*)a);
}

TEST_F(RzallocTest, RegionExpansionAllocatesMoreObjects) {
  const int initial_objects = 128;  // DEFAULT_OBJECTS_PER_REGION

  std::vector<TestObject*> objs;
  objs.reserve(initial_objects + 10);

  for (int i = 0; i < initial_objects; i++) {
    objs.push_back((TestObject*)arena_malloc(&arena));
  }

  // All initial allocations succeeded
  for (auto* p : objs) {
    EXPECT_NE(p, nullptr);
  }

  // Trigger new region
  TestObject* extra = (TestObject*)arena_malloc(&arena);
  EXPECT_NE(extra, nullptr);
}

TEST_F(RzallocTest, ClearResetsArenaState) {
  TestObject* a = (TestObject*)arena_malloc(&arena);
  ASSERT_NE(a, nullptr);

  arena_clear(&arena);

  EXPECT_EQ(arena_current_num_objects(&arena), 0u);
  EXPECT_EQ(arena.last_region, nullptr);
  EXPECT_EQ(arena.next_alloc_ptr, nullptr);
}

TEST_F(RzallocTest, StressTestManyAllocFreeCycles) {
  const int cycles = 5000;

  for (int i = 0; i < cycles; i++) {
    TestObject* o = (TestObject*)arena_malloc(&arena);
    ASSERT_NE(o, nullptr);

    o->x = i;
    o->y = i * 2;

    ASSERT_TRUE(arena_free(&arena, o));
  }

  EXPECT_EQ(arena_current_num_objects(&arena), 0u);
}

TEST_F(RzallocTest, StressTestMixedPatterns) {
  const int N = 2000;
  std::vector<TestObject*> allocated;

  for (int i = 0; i < N; i++) {
    TestObject* o = (TestObject*)arena_malloc(&arena);
    ASSERT_NE(o, nullptr);
    allocated.push_back(o);

    // Random-ish free pattern
    if (i % 3 == 0) {
      ASSERT_TRUE(arena_free(&arena, allocated.back()));
      allocated.pop_back();
    }
  }

  // Cleanup
  for (auto* o : allocated) {
    arena_free(&arena, o);
  }

  EXPECT_EQ(arena_current_num_objects(&arena), 0u);
}
