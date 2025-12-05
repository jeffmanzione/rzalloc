#include "rzalloc/rzalloc.h"

#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

//
// ALIGN(n, a)
//   Round `n` up to the nearest multiple of alignment `a`.
//
// ALIGN_UP(n)
//   Round up to the natural alignment required for max_align_t.
//   This guarantees that any type can be safely stored.
//
// SIZE_WITH_OFFSET(T)
//   Size of T, rounded up so that the next object following T will also
//   be correctly aligned.
//
#define ALIGN(n, a) (((n) + (a - 1)) & ~(a - 1))
#define ALIGN_UP(n) ALIGN(n, alignof(max_align_t))
#define SIZE_WITH_OFFSET(type) ALIGN_UP(sizeof(type))

// Number of objects each region can hold before allocating another region.
#define DEFAULT_OBJECTS_PER_REGION 128

//
// Per-object metadata stored directly before the user’s allocation.
// A freed object is pushed onto a LIFO free list for reuse.
//
struct ObjectContext_ {
  bool freed;
  ObjectContext *prev_freed;  // Next object on the free list
};

//
// Each region is a single contiguous allocation containing:
//
//   [RegionContext][ObjectContext+Object][ObjectContext+Object]...
//
// Regions are chained together (linked list), newest first.
//
typedef struct RegionContext_ {
  void *prev_region;  // Pointer to the previously allocated region
  size_t size;        // Total bytes available for objects in this region
} RegionContext;

//
// Allocate a new region capable of storing DEFAULT_OBJECTS_PER_REGION
// objects, each with their ObjectContext overhead.
//
// Returns a pointer to the start of the allocation, where a RegionContext
// is placed.
//
static void *allocate_region(size_t alloc_size, void *prev_region) {
  const size_t allocation_area_size = alloc_size * DEFAULT_OBJECTS_PER_REGION;

  // Prevent overflow in multiplication.
  if (alloc_size != 0 &&
      allocation_area_size / alloc_size != DEFAULT_OBJECTS_PER_REGION) {
    return NULL;
  }

  // Allocate space for the region header + object storage.
  void *region = malloc(SIZE_WITH_OFFSET(RegionContext) + allocation_area_size);
  if (region == NULL) {
    return NULL;
  }

  // Place the RegionContext at the start of the region.
  RegionContext *ctx = (RegionContext *)region;
  ctx->size = allocation_area_size;
  ctx->prev_region = prev_region;

  return region;
}

//
// Free all regions in the chain.
// Each region contains a pointer to its previous region.
//
static void free_region(void *region) {
  while (region != NULL) {
    RegionContext *ctx = (RegionContext *)region;
    void *prev = ctx->prev_region;
    free(region);
    region = prev;
  }
}

//
// Initialize an arena for allocations of size `object_size`.
// Returns true on success.
//
bool arena_init(RzallocArena *arena, size_t object_size) {
  // Store raw size and aligned size for user-visible and internal use.
  arena->raw_object_size = object_size;
  arena->object_size = ALIGN_UP(object_size);

  // Each allocation includes its ObjectContext and must be fully aligned.
  arena->alloc_size = SIZE_WITH_OFFSET(ObjectContext) + arena->object_size;

  // Allocate the first region.
  arena->last_region = allocate_region(arena->alloc_size, NULL);
  if (arena->last_region == NULL) {
    memset(arena, 0, sizeof(*arena));
    return false;
  }

  // Compute the start and end of the usable allocation area.
  arena->next_alloc_ptr =
      ((char *)arena->last_region) + SIZE_WITH_OFFSET(RegionContext);

  arena->end_alloc_ptr = (char *)arena->last_region +
                         SIZE_WITH_OFFSET(RegionContext) +
                         ((RegionContext *)arena->last_region)->size;

  arena->last_freed = NULL;
  arena->current_num_objects = 0;
  return true;
}

//
// Free all regions and reset the arena state.
//
void arena_clear(RzallocArena *arena) {
  if (arena == NULL) {
    return;
  }

  free_region(arena->last_region);

  arena->last_region = NULL;
  arena->next_alloc_ptr = NULL;
  arena->end_alloc_ptr = NULL;
  arena->last_freed = NULL;
  arena->current_num_objects = 0;
}

//
// Allocate a single object from the arena.
// This first tries to reuse an object from the free list; if none are
// available, it uses bump allocation from the current region.
//
void *arena_malloc(RzallocArena *arena) {
  if (arena == NULL) {
    return NULL;
  }
  arena->current_num_objects++;

  //
  // FAST-PATH: reuse a freed object.
  //
  if (arena->last_freed != NULL) {
    ObjectContext *free_spot = arena->last_freed;
    arena->last_freed = free_spot->prev_freed;  // Pop from free list

    free_spot->freed = false;
    free_spot->prev_freed = NULL;

    // User pointer begins after the ObjectContext header.
    return ((char *)free_spot) + SIZE_WITH_OFFSET(ObjectContext);
  }

  //
  // If current region is full, allocate a new region.
  //
  if (arena->next_alloc_ptr == arena->end_alloc_ptr) {
    void *new_region = allocate_region(arena->alloc_size, arena->last_region);
    if (new_region == NULL) {
      return NULL;  // Out of memory
    }

    arena->last_region = new_region;

    arena->next_alloc_ptr =
        (char *)arena->last_region + SIZE_WITH_OFFSET(RegionContext);

    arena->end_alloc_ptr = (char *)arena->last_region +
                           SIZE_WITH_OFFSET(RegionContext) +
                           ((RegionContext *)arena->last_region)->size;
  }

  //
  // Allocate object at next bump pointer.
  //
  ObjectContext *ctx = (ObjectContext *)arena->next_alloc_ptr;
  ctx->freed = false;
  ctx->prev_freed = NULL;

  // The allocation returned to the user sits immediately after the header.
  void *ptr = (char *)arena->next_alloc_ptr + SIZE_WITH_OFFSET(ObjectContext);

  // Advance bump pointer.
  arena->next_alloc_ptr = (char *)arena->next_alloc_ptr + arena->alloc_size;

  return ptr;
}

//
// Allocate and zero-initialize memory via the arena.
//
void *arena_calloc(RzallocArena *arena) {
  void *ptr = arena_malloc(arena);
  memset(ptr, 0, arena->object_size);
  return ptr;
}

//
// Free an object allocated by the arena.
// Objects are never returned to the system—only reused via a free list.
//
bool arena_free(RzallocArena *arena, void *ptr) {
  if (arena == NULL || ptr == NULL) {
    return false;
  }

  // Derive the metadata header located directly before the user object.
  ObjectContext *ctx =
      (ObjectContext *)((char *)ptr - SIZE_WITH_OFFSET(ObjectContext));

  if (ctx->freed) {
    // Double free attempt — ignore.
    return false;
  }

  ctx->freed = true;

  // Push onto the LIFO free list for quick reuse.
  ctx->prev_freed = arena->last_freed;
  arena->last_freed = ctx;

  arena->current_num_objects--;
  return true;
}

//
// Return original object size requested by user.
//
uint32_t arena_object_size(RzallocArena *arena) {
  if (arena == NULL) {
    return 0;
  }
  return arena->raw_object_size;
}

//
// Get the number of currently allocated (not freed) objects.
//
uint32_t arena_current_num_objects(RzallocArena *arena) {
  if (arena == NULL) {
    return 0;
  }
  return arena->current_num_objects;
}
