#include <stdio.h>
#include <string.h>

#include "rzalloc/rzalloc.h"

// A simple struct to allocate inside the arena
typedef struct {
  int id;
  char name[32];
} MyObject;

int main(void) {
  RzallocArena arena;

  //
  // 1. Initialize arena to allocate objects of size MyObject.
  //
  if (!arena_init(&arena, sizeof(MyObject))) {
    fprintf(stderr, "Failed to initialize arena\n");
    return 1;
  }

  printf("Arena initialized for objects of size: %u bytes\n",
         arena_object_size(&arena));

  //
  // 2. Allocate objects using arena_malloc()
  //
  MyObject *a = arena_malloc(&arena);
  MyObject *b = arena_malloc(&arena);

  a->id = 1;
  strcpy(a->name, "Alpha");

  b->id = 2;
  strcpy(b->name, "Beta");

  printf("Allocated A: { id=%d, name=%s }\n", a->id, a->name);
  printf("Allocated B: { id=%d, name=%s }\n", b->id, b->name);

  printf("Current live objects: %u\n", arena_current_num_objects(&arena));

  //
  // 3. Free one object; it goes onto the free list.
  //
  arena_free(&arena, b);
  printf("Freed B\n");
  printf("Current live objects: %u\n", arena_current_num_objects(&arena));

  //
  // 4. Allocate again — should reuse the previously freed slot.
  //
  MyObject *c = arena_malloc(&arena);
  c->id = 3;
  strcpy(c->name, "Gamma");

  printf("Allocated C (should reuse B's slot): { id=%d, name=%s }\n", c->id,
         c->name);

  //
  // 5. arena_calloc() returns zero-initialized memory
  //
  MyObject *d = arena_calloc(&arena);
  printf("Allocated D via calloc: { id=%d, name=\"%s\" }\n", d->id, d->name);

  //
  // 6. Print current stats
  //
  printf("Current live objects: %u\n", arena_current_num_objects(&arena));

  //
  // 7. Clear the entire arena – frees all regions at once.
  //
  arena_clear(&arena);
  printf("Arena cleared.\n");

  return 0;
}
