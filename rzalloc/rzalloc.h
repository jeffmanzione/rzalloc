#ifndef JEFFMANZIONE_RZALLOC_RZALLOC_H_
#define JEFFMANZIONE_RZALLOC_RZALLOC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// Rzalloc: Region/Zoned Memory Allocator
//
// This module implements a fast arena-style allocator that uses chained
// regions (zones) for bulk allocation. Each allocated object has a small
// header allowing it to be individually freed. Freed objects are pushed onto
// a LIFO free list and reused before new space is taken from the region.
//
// Characteristics:
//   • Very fast allocation (pointer bump) when free-list not used
//   • Very fast deallocation (constant-time free-list push)
//   • Memory is grouped into regions; freeing the arena frees all regions
//   • Individual frees DO NOT return memory to the system
//   • Good for fixed-size objects, object pools, and high-churn alloc patterns
//

// Forward declaration of internal per-object metadata.
typedef struct ObjectContext_ ObjectContext;

//
// RzallocArena
//
// Represents a single arena that allocates fixed-size objects.
// The arena grows by allocating new regions as needed. Each region stores
// `DEFAULT_OBJECTS_PER_REGION` allocations, plus metadata.
//
// Fields are public only so that the .c file can access them; users should
// treat this structure as opaque and only manipulate it using API functions.
//
typedef struct {
  // Pointer to the most recently allocated region.
  void *last_region;

  // Size of the user-requested object type (un-aligned).
  size_t raw_object_size;

  // Object size rounded up to max_align_t alignment.
  size_t object_size;

  // Total bytes per allocation: ObjectContext + aligned object.
  size_t alloc_size;

  // Pointer to the next available space inside the newest region.
  void *next_alloc_ptr;

  // End pointer of the latest region’s allocation area.
  void *end_alloc_ptr;

  // Head of the free-list for individual freed objects.
  ObjectContext *last_freed;

  // Count of currently allocated (not freed) objects.
  uint32_t current_num_objects;

} RzallocArena;

//
// arena_init
//   Initialize an arena for objects of size `object_size`.
//
// Parameters:
//   arena        - pointer to a user-provided RzallocArena
//   object_size  - size of the payload object in bytes
//
// Returns:
//   true on success, false on memory allocation failure.
//
// Notes:
//   • Must be called before any other arena_* function.
//   • The arena must be cleared with arena_clear() when finished.
//
bool arena_init(RzallocArena *arena, size_t object_size);

//
// arena_clear
//   Free all regions belonging to the arena, reset the arena to zero-state.
//
// Parameters:
//   arena  - arena to clear
//
// Notes:
//   • Frees all allocated regions in one shot.
//   • After calling this, the arena must be re-initialized with arena_init()
//
void arena_clear(RzallocArena *arena);

//
// arena_malloc
//   Allocate a single object from the arena.
//
// Behavior:
//   • If objects have been freed, reuse from the free-list.
//   • Otherwise, allocate from the current region.
//   • If the region is full, allocates a new region automatically.
//
// Returns:
//   Pointer to the allocated object, or NULL on OOM.
//
void *arena_malloc(RzallocArena *arena);

//
// arena_calloc
//   Same as arena_malloc() but zero-initializes the allocated object.
//
// Returns:
//   Pointer to the allocated object, or NULL on OOM.
//
void *arena_calloc(RzallocArena *arena);

//
// arena_free
//   Free an object allocated by the arena.
//
// Behavior:
//   • Pushes the object onto a free-list for future reuse.
//   • Does NOT return memory to the system.
//   • Double frees return false but otherwise do not break the arena.
//
// Parameters:
//   arena  - arena owning the object
//   ptr    - pointer returned from arena_malloc or arena_calloc
//
// Returns:
//   true if successfully freed,
//   false if `ptr` was already freed or if inputs are invalid.
//
bool arena_free(RzallocArena *arena, void *ptr);

//
// arena_object_size
//   Retrieve the original unaligned object size used when initializing
//   the arena.
//
uint32_t arena_object_size(RzallocArena *arena);

//
// arena_current_num_objects
//   Number of currently live objects (allocations minus frees).
//
uint32_t arena_current_num_objects(RzallocArena *arena);

#ifdef __cplusplus
}
#endif

#endif /* JEFFMANZIONE_RZALLOC_RZALLOC_H_ */
