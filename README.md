

# HIF — Hyperedge Inclusion Forest

# HIF — Hyperedge Inclusion Forest

<p align="center">
  <img src="https://img.shields.io/badge/Hyperedge-Inclusion_Forest-purple?style=for-the-badge" alt="Hyperedge Inclusion Forest">
  <img src="https://img.shields.io/badge/language-C-00599C?style=for-the-badge&logo=c&logoColor=white" alt="C">
  <img src="https://img.shields.io/badge/standard-C11-blue?style=for-the-badge" alt="C11">
  <img src="https://img.shields.io/badge/memory-ASan%2FLSan%2FValgrind_clean-brightgreen?style=for-the-badge" alt="Memory checked">
  <img src="https://img.shields.io/badge/license-MIT-green?style=for-the-badge" alt="MIT License">
</p>

> A pure C data structure for organizing weighted hyperedges into an inclusion-based hierarchy.  
> Or, less academically: **a forest where big important sets adopt smaller weaker sets**.

> A pure C data structure for organizing weighted hyperedges into an inclusion-based hierarchy.
>
> Or, less academically: **a forest where big important sets adopt smaller weaker sets**.

![C](https://img.shields.io/badge/language-C-blue)
![License: MIT](https://img.shields.io/badge/license-MIT-green)
![Memory checks](https://github.com/Grey0ne-dev/hiperedge-inclusion-forest/actions/workflows/memory.yml/badge.svg)

---

## What is HIF?

**HIF** stands for **Hyperedge Inclusion Forest**.

It stores weighted hyperedges in a forest where every parent-child relationship must satisfy:

```text
child.vertices ⊆ parent.vertices
parent.weight >= child.weight
````

So a heavier superset can become the parent of a lighter subset.

Example:

```text
{1,2,3,4} w=10.0
├── {1,2,3} w=7.0
│   └── {1,2} w=3.0
└── {3,4} w=2.0
```

Disjoint or incomparable hyperedges become separate roots.

No fake hierarchy.
No “everything is secretly a tree” cope.
Only valid inclusion relationships.

---

## Why?

Because sometimes a normal graph is not enough.

Sometimes your data looks like this:

```text
weighted sets
subset/superset relationships
overlapping clusters
dominant elements
peripheral elements
hierarchical decomposition
```

And then ordinary adjacency lists start sweating.

HIF is useful when you need to organize weighted hyperedges by:

* inclusion
* dominance
* weight
* overlap
* subset/superset queries
* hierarchical clustering-like structure

---

## Features

* Pure C implementation
* Weighted hyperedge insertion
* Inclusion-based forest construction
* Superset queries
* Subset queries
* Top-k heaviest hyperedges
* Weight threshold search
* Weight range search
* Similarity search
* Forest rebalancing
* Duplicate merging
* Weight pruning
* Binary save/load
* BFS / DFS / weight-order traversal
* Explicit memory ownership rules
* Designed to be checked with ASan, LSan, and Valgrind

---

## Core invariant

A node can be a parent only if:

```text
parent is a superset of child
parent is at least as heavy as child
```

Formally:

```text
child ⊆ parent
parent.weight >= child.weight
```

The library exposes:

```c
int verify_forest(Forest *f);
```

Use it when you want to ask:

> “Is my forest still sane?”

If it returns `1`, the structure is valid.

If it returns `0`, the trees are lying.

---

## Basic usage

```c
#include "hif.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    Forest *f = forest_create();

    int a[] = {1, 2, 3, 4};
    int b[] = {1, 2, 3};
    int c[] = {1, 2};
    int d[] = {8, 9};

    insert_hyperedge(f, a, 4, 10.0);
    insert_hyperedge(f, b, 3, 7.0);
    insert_hyperedge(f, c, 2, 3.0);
    insert_hyperedge(f, d, 2, 100.0);

    print_forest(f);

    int query[] = {1, 2};

    Node *boss = find_minimal_superset(f, query, 2);

    if (boss) {
        printf("Minimal superset weight: %.2f\n", boss->he.weight);
    }

    forest_free(f);
    return 0;
}
```

Expected emotional result:

```text
The forest accepts your hyperedges.
Some become roots.
Some become children.
One becomes suspiciously powerful.
```

---

## Public API overview

### Forest lifecycle

```c
Forest *forest_create(void);
void forest_free(Forest *f);
```

Create a forest.
Free the forest.

Yes, freeing is included.

This is C. The garbage collector fairy is not coming.

---

### Insertion

```c
void insert_hyperedge(
    Forest *f,
    const int *verts,
    int nverts,
    double weight
);
```

The input vertex array is sorted and deduplicated internally.

So this:

```c
int e[] = {3, 1, 2, 2, 3};
```

becomes:

```text
{1,2,3}
```

---

### Batch building

```c
void forest_insert_batch(Forest *f, Hyperedge *edges, int nedges);
Forest *forest_build_bulk(Hyperedge *edges, int nedges);
```

Batch insertion sorts hyperedges by descending weight before insertion.

Why?

Because inserting stronger hyperedges first usually creates a cleaner hierarchy.

Big bosses enter first.
Small goblins find their place later.

---

## Queries

### Top-k heaviest nodes

```c
Node **find_top_k(Forest *f, int k, int *result_count);
```

Returns the `k` heaviest hyperedges.

The returned array must be freed by the caller.

```c
int count = 0;
Node **top = find_top_k(f, 10, &count);

/* use top[i] */

free(top);
```

Do **not** free the nodes inside.

```c
free(top);       /* correct */
free(top[0]);    /* forbidden malloc necromancy */
```

---

### Superset queries

```c
Node *find_minimal_superset(Forest *f, const int *query, int nquery);
Node *find_heaviest_superset(Forest *f, const int *query, int nquery);

Node **find_all_supersets(
    Forest *f,
    const int *query,
    int nquery,
    int *result_count
);
```

Ask questions like:

```text
Who contains this set?
Who is the smallest container?
Who is the heaviest container?
Who are all the containers?
```

---

### Subset queries

```c
Node **find_all_subsets(
    Forest *f,
    const int *query,
    int nquery,
    int *result_count
);
```

Find all nodes whose vertex sets are subsets of the query.

In other words:

```text
Which stored hyperedges fit inside this set?
```

---

### Weight queries

```c
int find_by_weight_threshold(Forest *f, double threshold);

Node **get_clusters_by_weight(
    Forest *f,
    double threshold,
    int *cluster_count
);

Node **find_by_weight_range(
    Forest *f,
    double min_weight,
    double max_weight,
    int *result_count
);
```

Use these when you only care about hyperedges with enough aura.

```text
weight >= threshold
```

or:

```text
min_weight <= weight <= max_weight
```

---

### Similarity search

```c
Node **find_k_most_similar(
    Forest *f,
    const int *query,
    int nquery,
    int k,
    int *result_count
);
```

Similarity is computed using the overlap coefficient:

```text
|A ∩ B| / min(|A|, |B|)
```

Human translation:

> “How much do these two hyperedges spiritually overlap?”

---

## Maintenance operations

```c
void forest_rebalance(Forest *f);

int forest_merge_duplicates(Forest *f, int keep_max);

int forest_prune_by_weight(Forest *f, double threshold);

void forest_optimize(Forest *f);
```

### `forest_rebalance`

Collects all nodes, sorts them by weight, detaches children, and rebuilds the forest.

This is the “go clean your room” operation.

---

### `forest_merge_duplicates`

Merges hyperedges with identical vertex sets.

```c
forest_merge_duplicates(f, 1);
```

With `keep_max = 1`, duplicate hyperedges keep the maximum weight.

```c
forest_merge_duplicates(f, 0);
```

With `keep_max = 0`, duplicate hyperedges use average weight.

---

### `forest_prune_by_weight`

Removes nodes whose weight is below a threshold.

```c
forest_prune_by_weight(f, 5.0);
```

Everything weaker than `5.0` gets sent to the shadow realm.

---

### `forest_optimize`

```c
void forest_optimize(Forest *f);
```

Does:

```text
merge duplicates
rebalance forest
```

A spa day for your hypergraph.

---

## Traversal

```c
typedef int (*NodeVisitor)(Node *node, void *user_data);

void forest_traverse_bfs(Forest *f, NodeVisitor visitor, void *user_data);
void forest_traverse_dfs(Forest *f, NodeVisitor visitor, void *user_data);
void forest_traverse_by_weight(Forest *f, NodeVisitor visitor, void *user_data);
```

Traversal callbacks return:

```text
0       continue traversal
nonzero stop traversal
```

So your visitor can say:

```text
nice node, continue
```

or:

```text
I have seen enough
```

---

## Serialization

```c
int forest_save(Forest *f, const char *filename);
Forest *forest_load(const char *filename);
```

Binary save/load.

Not JSON.

Because sometimes you want storage, not a JavaScript panic attack.

Example:

```c
forest_save(f, "forest.hif");

Forest *loaded = forest_load("forest.hif");
```

---

## Ownership rules

This library follows classic C ownership rules:

```text
If HIF creates the forest, HIF frees the forest.
If HIF returns an array, caller frees the array.
If HIF returns nodes inside an array, caller does NOT free the nodes.
```

Correct:

```c
int count = 0;
Node **result = find_all_supersets(f, query, nquery, &count);

/* use result[i] */

free(result);
forest_free(f);
```

Incorrect:

```c
free(result[0]);  /* no */
```

---

## Complexity

Let:

```text
n = total number of hyperedges / nodes
m = number of vertices in the inserted/query hyperedge
c = number of candidates returned by the vertex index
k = requested top-k count
t = number of elements passing a weight threshold
```

### Core operations

| Operation                    |                     Complexity |
| ---------------------------- | -----------------------------: |
| Create forest                |                         `O(1)` |
| Free forest                  |        `O(n + total_vertices)` |
| Insert hyperedge             | `O(m log m + c·m + placement)` |
| Insert hyperedge, worst case |                       `O(n·m)` |
| Batch insert                 |        `O(b log b + b·insert)` |
| Count nodes                  |                         `O(1)` |
| Max depth                    |                         `O(n)` |
| Verify forest                |            `O(n·m)` worst case |

### Query operations

| Operation                       |                         Complexity |
| ------------------------------- | ---------------------------------: |
| Find minimal superset           |                 `O(m log m + c·m)` |
| Find heaviest superset          |                 `O(m log m + c·m)` |
| Find all supersets              |                 `O(m log m + c·m)` |
| Find all subsets                |                 `O(m log m + n·m)` |
| Find top-k                      | `O(k)` after weight index is built |
| Find top-k after mutation       |                   `O(n log n + k)` |
| Weight threshold query          | `O(t)` after weight index is built |
| Weight threshold after mutation |                   `O(n log n + t)` |
| Weight range query              | `O(n)` after weight index is built |
| Similarity search               |       `O(m log m + n·m + n log n)` |

### Maintenance operations

| Operation        |                        Complexity |
| ---------------- | --------------------------------: |
| Rebalance        |        `O(n log n + n·placement)` |
| Merge duplicates |                         `O(n²·m)` |
| Prune by weight  |          `O(n + rebuild_indexes)` |
| Optimize         | `O(merge_duplicates + rebalance)` |
| Rebuild indexes  |           `O(n + total_vertices)` |

### Serialization and traversal

| Operation              |                                 Complexity |
| ---------------------- | -----------------------------------------: |
| Save forest            |                    `O(n + total_vertices)` |
| Load forest            | `O(n + total_vertices)` plus index rebuild |
| BFS traversal          |                                     `O(n)` |
| DFS traversal          |                                     `O(n)` |
| Weight-order traversal |                               `O(n log n)` |

---

## Space complexity

Approximate memory usage:

```text
O(n + total_vertices + total_child_links + index_entries)
```

More concretely:

| Component             |                                   Space |
| --------------------- | --------------------------------------: |
| Nodes                 |                                  `O(n)` |
| Stored vertex arrays  |                     `O(total_vertices)` |
| Children arrays       |                  `O(total_child_links)` |
| Root array            |                                  `O(r)` |
| Root heap             |                                  `O(r)` |
| Global node index     |                                  `O(n)` |
| Lazy weight order     |                                  `O(n)` |
| Vertex inverted index | `O(total_vertices + distinct_vertices)` |

This means HIF intentionally spends extra memory on indexes to make common queries faster.

Classic tradeoff:

```text
more memory
less crying during queries
```

---

## Build

Simple build:

```bash
gcc -std=c11 -Wall -Wextra -pedantic -O2 \
    hif.c main.c \
    -o hif_demo
```

Debug build:

```bash
gcc -std=c11 -Wall -Wextra -pedantic -g -O0 \
    hif.c main.c \
    -o hif_demo
```

Sanitizer build:

```bash
gcc -std=c11 -Wall -Wextra -pedantic -g -O0 \
    -fsanitize=address,leak,undefined \
    -fno-omit-frame-pointer \
    hif.c main.c \
    -o hif_demo
```

Run with leak detection:

```bash
ASAN_OPTIONS=detect_leaks=1 ./hif_demo
```

Valgrind:

```bash
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --errors-for-leak-kinds=definite,possible \
    --error-exitcode=1 \
    ./hif_demo
```

---

## GitHub Actions memory badge

Recommended badge:

```md
![Memory checks](https://github.com/YOUR_NAME/YOUR_REPO/actions/workflows/memory.yml/badge.svg)
```

Recommended wording:

```text
Memory checked with ASan, LSan, and Valgrind on the test suite.
```

Avoid saying:

```text
Memory leak free forever.
```

That is not a CI badge.

That is a religious claim.

---

## Example GitHub Actions workflow

Create:

```text
.github/workflows/memory.yml
```

Example:

```yaml
name: Memory checks

on:
  push:
  pull_request:

jobs:
  sanitizer:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Build with sanitizers
        run: |
          gcc -std=c11 -Wall -Wextra -pedantic -g -O0 \
              -fsanitize=address,leak,undefined \
              -fno-omit-frame-pointer \
              hif.c tests/test_hif.c \
              -o test_hif

      - name: Run sanitizer tests
        run: |
          ASAN_OPTIONS=detect_leaks=1 ./test_hif

  valgrind:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Install Valgrind
        run: |
          sudo apt-get update
          sudo apt-get install -y valgrind

      - name: Build
        run: |
          gcc -std=c11 -Wall -Wextra -pedantic -g -O0 \
              hif.c tests/test_hif.c \
              -o test_hif

      - name: Run under Valgrind
        run: |
          valgrind \
              --leak-check=full \
              --show-leak-kinds=all \
              --errors-for-leak-kinds=definite,possible \
              --error-exitcode=1 \
              ./test_hif
```

---

## When should I use HIF?

Use HIF when you have:

* weighted hyperedges
* set inclusion relationships
* need for hierarchy
* need for subset/superset queries
* overlapping sets
* dominance-like structure
* clustering-like decomposition
* graph data that refuses to behave like a normal graph

Do not use HIF when you only need:

* a normal graph
* a plain hash set
* a vector
* a linked list with academic cosplay
* vibes

---

## Project philosophy

HIF believes in four laws:

```text
1. Parents must contain their children.
2. Parents must not be weaker than their children.
3. Returned arrays must be freed.
4. C is powerful, but it is also a loaded crossbow pointed at your foot.
```

---

## License

MIT License.

Use it.
Fork it.
Break it.
Fix it.
Put it in your research project.
Put it in your compiler.
Put it in your cursed graph algorithm.

Just remember:

```c
forest_free(f);
```

Seriously.

Free the forest.

```
