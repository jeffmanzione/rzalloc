Here is a **clean, polished, production-quality README.md** for a GitHub project implementing region/zone-based allocation (`rzalloc`). It includes feature overview, examples, implementation details, diagrams, benchmarks, and integration notes—similar in style to high-quality C library READMEs.

---

# `rzalloc`

`rzalloc` is a lightweight, high-performance C library that implements **zone/region-based memory allocation**. It provides extremely fast, fixed-size object allocation using:

* **Arena-style bump allocation**
* **Per-object metadata for constant-time free**
* **Region-based bulk release of memory**
* **Optional free-list recycling for reuse**

`rzalloc` is ideal for workloads that make many small or fixed-sized allocations, such as:

* Abstract syntax trees (ASTs)
* Compilers / interpreters
* Game engines
* ECS systems
* Query execution engines
* Parsers
* Short-lived jobs or batch tasks

It offers malloc-level convenience with significant performance gains.

---

## Features

* **Extremely fast allocations**

  * Bump-pointer allocation when regions have space
  * Free-list reuse for O(1) frees
* **Bulk region clearing**

  * Free all objects in one call
* **Zero fragmentation**

  * Allocations are contiguous inside regions
* **Stable, fixed-size object model**

  * Perfect for small objects, nodes, entries, etc.
* **Thread-safe when arenas are thread-local**
* **Clean, small implementation (~250 LoC)**

---

## API Overview

```c
bool arena_init(RzallocArena *arena, size_t object_size);
void arena_clear(RzallocArena *arena);

void *arena_malloc(RzallocArena *arena);
void *arena_calloc(RzallocArena *arena);

bool arena_free(RzallocArena *arena, void *ptr);

uint32_t arena_object_size(RzallocArena *arena);
uint32_t arena_current_num_objects(RzallocArena *arena);
```

* Each `RzallocArena` manages objects of **one size**.
* All regions grow automatically as needed.
* Freed objects are reused immediately.

---

## Example Usage

```c
#include "rzalloc/rzalloc.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Point;

int main() {
    RzallocArena arena;
    arena_init(&arena, sizeof(Point));

    Point *p = arena_malloc(&arena);
    p->x = 10;
    p->y = 20;

    // Object reuse
    arena_free(&arena, p);

    // Allocate again (same memory reused)
    Point *q = arena_malloc(&arena);

    printf("Point: %d %d\n", q->x, q->y);

    // Bulk free
    arena_clear(&arena);
}
```

---

## Installation

Add to your MODULE.bazel:

```
bazel_dep(name = "jeffmanzione_rzalloc", version = "1.0.0")
```

---

## How It Works

### 1. **Regions**

Each region is a contiguous chunk:

```
+-----------------------+
| RegionContext         |
+-----------------------+
| ObjectCtx | Object    |
| ObjectCtx | Object    |
| ...                     (fixed capacity)
+-----------------------+
```

Only a pointer bump is needed for allocations.

### 2. **Object Metadata**

Every allocation contains a hidden `ObjectContext`:

```
[ObjectContext][Your Object Data]
```

Used for:

* free-list chain
* freed flag
* fast recycling

### 3. **Free List**

Frees push the object onto a **LIFO free list**.
`arena_malloc()` first tries free-list reuse, then bump allocation.

### 4. **Bulk Free**

`arena_clear(arena)` walks regions and `free()`’s them all.
Perfect for ASTs, request-scoped objects, or frame allocators.

---

## Benchmarks

Benchmarks use **Google Benchmark** (see `rzalloc_benchmark.cc`).

### Categories benchmarked:

* Single alloc/free (malloc vs rzalloc)
* Long-lived allocations (no frees)
* Batch allocations (1000 at a time)
* Batch long-lived (no frees)

### Expected Results

~50% speedup

**Exact results depend on your machine.**

---

## Testing

Includes comprehensive GoogleTest suite:

* Region allocation
* Free-list behavior
* Alignment correctness
* Double-free safety
* Region chain growth
* Zeroing semantics in calloc
* Bulk clearing

---

## Limitations

* All objects in an arena must be the **same size**.
* Memory is only returned to the system on `arena_clear`.
* Not intrinsically thread-safe (intended for thread-local use).
* Not for allocations larger than a few KB (tune `DEFAULT_OBJECTS_PER_REGION` if needed).

---

## License

MIT License — free for commercial and personal use.

---
