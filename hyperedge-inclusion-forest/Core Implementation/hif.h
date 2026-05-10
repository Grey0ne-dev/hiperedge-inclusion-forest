/**
 * Hyperedge Inclusion Forest (HIF)
 * Weight-Based Hierarchical Hypergraph Decomposition
 *
 * A novel data structure that organizes weighted hypergraphs by:
 * - PRIMARY:   Weight (heavier edges rise to roots)
 * - SECONDARY: Subset relationships (maintain lattice structure)
 * - TERTIARY:  Overlap (cluster similar hyperedges)
 *
 * Result: Natural hierarchical decomposition with:
 *   ✓ Dominant elements at roots
 *   ✓ Peripheral elements at leaves
 *   ✓ Overlapping elements clustered
 *   ✓ Disjoint elements separated
 *
 * Copyright (c) 2024
 * Licensed under MIT License
 */

#ifndef HYPEREDGE_INCLUSION_FOREST_H
#define HYPEREDGE_INCLUSION_FOREST_H

#include <stddef.h>

/* ========== TYPE DEFINITIONS ========== */

typedef struct {
    int    *verts;   /* Sorted array of vertex IDs      */
    int     nverts;  /* Number of vertices               */
    double  weight;  /* Weight (importance/score)        */
} Hyperedge;

typedef struct Node {
    Hyperedge      he;
    struct Node  **children;
    int            nchildren;
    int            children_cap;
} Node;

/*
 * Max-heap of Node pointers ordered by weight (heaviest at top).
 * Used internally to support true O(k log k) find_top_k queries.
 */
typedef struct {
    Node **data;
    int    size;
    int    cap;
} NodeHeap;

typedef struct {
    Node    **roots;
    int       nroots;
    int       roots_cap;
    NodeHeap *root_heap;  /* always-valid heap over current roots */
} Forest;

/* ========== HEAP API ========== */

/**
 * Create an empty max-heap (ordered by node weight).
 */
NodeHeap *heap_create(void);

/**
 * Free heap (does NOT free the nodes inside).
 */
void heap_free(NodeHeap *h);

/**
 * Push a node onto the heap. O(log n).
 */
void heap_push(NodeHeap *h, Node *nd);

/**
 * Pop and return the heaviest node. O(log n).
 * Returns NULL if heap is empty.
 */
Node *heap_pop(NodeHeap *h);

/* ========== CORE API ========== */

/**
 * Create a new empty forest.
 */
Forest *forest_create(void);

/**
 * Free forest and all nodes.
 */
void forest_free(Forest *f);

/**
 * Insert a hyperedge into the forest.
 *
 * Ordering strategy:
 *   1. Heavier weight  → becomes parent
 *   2. Equal weight    → subset relationship determines hierarchy
 *   3. Incomparable    → separate branch / new root
 *
 * @param f       Forest to insert into
 * @param verts   Array of vertex IDs (will be sorted & deduplicated internally)
 * @param nverts  Number of vertices
 * @param weight  Weight / importance score
 */
void insert_hyperedge(Forest *f, const int *verts, int nverts, double weight);

/**
 * Find top-k heaviest hyperedges.
 *
 * Uses a working max-heap seeded with all roots, expanding children
 * on each pop.  Complexity: O(k log k) time, O(k·b) extra space
 * where b is the maximum branching factor.
 *
 * Caller must free the returned array (but NOT the nodes inside).
 *
 * @param f             Forest to search
 * @param k             Number of top elements requested
 * @param result_count  Output: actual number returned (≤ k)
 * @return              Array of top-k nodes, or NULL if k == 0 / forest empty
 */
Node **find_top_k(Forest *f, int k, int *result_count);

/**
 * Count hyperedges with weight >= threshold.
 *
 * @param f         Forest to search
 * @param threshold Minimum weight (inclusive)
 * @return          Number of matching hyperedges
 */
int find_by_weight_threshold(Forest *f, double threshold);

/**
 * Find the minimal superset of a query set (fewest extra vertices).
 *
 * @param f      Forest to search
 * @param query  Sorted vertex array
 * @param nquery Number of vertices in query
 * @return       Node with smallest superset, or NULL if none exists
 */
Node *find_minimal_superset(Forest *f, const int *query, int nquery);

/**
 * Find the heaviest superset of a query set.
 *
 * @param f      Forest to search
 * @param query  Sorted vertex array
 * @param nquery Number of vertices in query
 * @return       Heaviest node that is a superset, or NULL
 */
Node *find_heaviest_superset(Forest *f, const int *query, int nquery);

/* ========== CLUSTERING & ANALYSIS ========== */

/**
 * Get all nodes whose weight >= threshold.
 * Returns roots of subtrees that satisfy the threshold.
 *
 * Caller must free the returned array.
 *
 * @param f             Forest
 * @param threshold     Weight cutoff
 * @param cluster_count Output: number of nodes returned
 * @return              Array of matching nodes
 */
Node **get_clusters_by_weight(Forest *f, double threshold, int *cluster_count);

/**
 * Compute the overlap coefficient between two nodes.
 * Returns |A ∩ B| / min(|A|, |B|)  in [0, 1].
 */
double compute_overlap(const Node *a, const Node *b);

/* ========== UTILITY ========== */

/** Total number of nodes across all trees. */
int    count_total_nodes(Forest *f);

/** Maximum depth of any tree in the forest. */
int    forest_max_depth(Forest *f);

/** Maximum weight in the forest. O(1) via root_heap. */
double forest_max_weight(Forest *f);

/** Minimum weight in the forest. O(n). */
double forest_min_weight(Forest *f);

/** Print a human-readable representation of the forest. */
void   print_forest(Forest *f);

/**
 * Verify the weight-monotonicity invariant:
 *   parent.weight >= child.weight for every edge in every tree.
 *
 * @return 1 if valid, 0 otherwise
 */
int verify_forest(Forest *f);

typedef struct {
    int    total_nodes;
    int    num_roots;
    int    max_depth;
    double max_weight;
    double min_weight;
    double avg_weight;
    int    max_children;
} ForestStats;

ForestStats get_forest_stats(Forest *f);

/* ========== ADVANCED QUERY OPERATIONS ========== */

/**
 * Find all supersets of a query set.
 * Caller must free the returned array.
 */
Node **find_all_supersets(Forest *f, const int *query, int nquery,
                          int *result_count);

/**
 * Find all subsets of a query set.
 * Caller must free the returned array.
 */
Node **find_all_subsets(Forest *f, const int *query, int nquery,
                        int *result_count);

/**
 * Find all nodes with weight in [min_weight, max_weight].
 * Caller must free the returned array.
 */
Node **find_by_weight_range(Forest *f, double min_weight, double max_weight,
                            int *result_count);

/**
 * Find all nodes whose vertex set contains every vertex in `vertices`.
 * Caller must free the returned array.
 */
Node **find_containing_vertices(Forest *f, const int *vertices, int nvertices,
                                int *result_count);

/**
 * Find the k nodes most similar to a query set.
 * Similarity = overlap coefficient (|A ∩ B| / min(|A|,|B|)).
 * Complexity: O(n log n).
 * Caller must free the returned array.
 */
Node **find_k_most_similar(Forest *f, const int *query, int nquery,
                           int k, int *result_count);

/* ========== OPTIMIZATION & MAINTENANCE ========== */

/**
 * Rebalance the forest.
 * Collects all nodes, sorts by weight descending, then reinserts.
 * Frees children arrays properly before rebuild.
 */
void forest_rebalance(Forest *f);

/**
 * Merge duplicate hyperedges (identical vertex sets, possibly different weights).
 *
 * @param f        Forest to deduplicate
 * @param keep_max 1 = keep maximum weight, 0 = use average weight
 * @return         Number of duplicates merged
 */
int forest_merge_duplicates(Forest *f, int keep_max);

/**
 * Remove all nodes with weight < threshold (recursive, including subtrees).
 *
 * @param f         Forest to prune
 * @param threshold Minimum weight to retain
 * @return          Number of nodes removed
 */
int forest_prune_by_weight(Forest *f, double threshold);

/**
 * Merge duplicates then rebalance for optimal query performance.
 */
void forest_optimize(Forest *f);

/* ========== BATCH OPERATIONS ========== */

/**
 * Insert multiple hyperedges.
 * Sorts by weight descending before inserting for a better tree shape.
 *
 * @param f      Forest to insert into
 * @param edges  Array of hyperedges (not modified)
 * @param nedges Number of edges
 */
void forest_insert_batch(Forest *f, Hyperedge *edges, int nedges);

/**
 * Build a new forest from an array of hyperedges.
 * Sorts by weight descending for optimal insertion order.
 *
 * @param edges  Array of hyperedges (not modified)
 * @param nedges Number of edges
 * @return       New forest (caller must forest_free)
 */
Forest *forest_build_bulk(Hyperedge *edges, int nedges);

/* ========== SERIALIZATION ========== */

/**
 * Save forest to a binary file.
 * @return 0 on success, -1 on error
 */
int forest_save(Forest *f, const char *filename);

/**
 * Load forest from a binary file.
 * @return Loaded forest, or NULL on error (caller must forest_free)
 */
Forest *forest_load(const char *filename);

/* ========== TRAVERSAL ========== */

/**
 * Visitor callback.  Return 0 to continue traversal, non-zero to stop.
 */
typedef int (*NodeVisitor)(Node *node, void *user_data);

/** Breadth-first traversal (level order). */
void forest_traverse_bfs(Forest *f, NodeVisitor visitor, void *user_data);

/** Depth-first traversal (pre-order). */
void forest_traverse_dfs(Forest *f, NodeVisitor visitor, void *user_data);

/** Traverse all nodes in descending weight order. O(n log n). */
void forest_traverse_by_weight(Forest *f, NodeVisitor visitor, void *user_data);

#endif /* HYPEREDGE_INCLUSION_FOREST_H */
