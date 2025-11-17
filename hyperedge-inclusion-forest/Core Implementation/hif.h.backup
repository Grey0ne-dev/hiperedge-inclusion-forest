/**
 * Hyperedge Inclusion Forest (HIF)
 * Weight-Based Hierarchical Hypergraph Decomposition
 * 
 * A novel data structure that organizes weighted hypergraphs by:
 * - PRIMARY: Weight (heavier edges rise to roots)
 * - SECONDARY: Subset relationships (maintain lattice structure)
 * - TERTIARY: Overlap (cluster similar hyperedges)
 * 
 * Result: Natural hierarchical decomposition with:
 * ✓ Dominant elements at roots
 * ✓ Peripheral elements at leaves
 * ✓ Overlapping elements clustered
 * ✓ Disjoint elements separated
 * 
 * Copyright (c) 2024
 * Licensed under MIT License
 */

#ifndef HYPEREDGE_INCLUSION_FOREST_H
#define HYPEREDGE_INCLUSION_FOREST_H

#include <stddef.h>

// ========== TYPE DEFINITIONS ==========

typedef struct {
    int *verts;      // Sorted array of vertex IDs
    int nverts;      // Number of vertices
    double weight;   // Weight (importance/centrality/score)
} Hyperedge;

typedef struct Node {
    Hyperedge he;
    struct Node **children;  // Children (lighter or subsets)
    int nchildren;
    int children_cap;
} Node;

typedef struct {
    Node **roots;      // Roots (heaviest/largest hyperedges)
    int nroots;
    int roots_cap;
} Forest;

// ========== CORE API ==========

/**
 * Create a new empty forest
 */
Forest *forest_create(void);

/**
 * Free forest and all nodes
 */
void forest_free(Forest *f);

/**
 * Insert a hyperedge into the forest
 * 
 * Ordering strategy:
 * 1. Heavier weights → become parents
 * 2. If weights equal → subset relationships determine hierarchy
 * 3. If incomparable → separate branches
 * 
 * @param f Forest to insert into
 * @param verts Array of vertex IDs (will be normalized)
 * @param nverts Number of vertices
 * @param weight Weight/importance/centrality score
 */
void insert_hyperedge(Forest *f, const int *verts, int nverts, double weight);

/**
 * Find top-k heaviest hyperedges
 * Returns array of nodes (caller must free)
 * 
 * Complexity: O(k) via BFS from roots
 * 
 * @param f Forest to search
 * @param k Number of top elements to return
 * @param result_count Output: actual number returned
 * @return Array of top-k nodes (NULL if k=0)
 */
Node **find_top_k(Forest *f, int k, int *result_count);

/**
 * Find all hyperedges with weight >= threshold
 * 
 * @param f Forest to search
 * @param threshold Minimum weight
 * @return Number of hyperedges found
 */
int find_by_weight_threshold(Forest *f, double threshold);

/**
 * Find the minimal superset of a query set
 * (Same as before, but now considers weight)
 * 
 * @param f Forest to search
 * @param query Query vertex set (normalized)
 * @param nquery Number of vertices in query
 * @return Node containing minimal superset, or NULL
 */
Node *find_minimal_superset(Forest *f, const int *query, int nquery);

/**
 * Find the heaviest superset of a query set
 * (New: prioritize weight over size)
 * 
 * @param f Forest to search
 * @param query Query vertex set
 * @param nquery Number of vertices
 * @return Heaviest node containing query, or NULL
 */
Node *find_heaviest_superset(Forest *f, const int *query, int nquery);

// ========== CLUSTERING & ANALYSIS ==========

/**
 * Get clusters at a specific weight threshold
 * Returns roots of subtrees where root.weight >= threshold
 * 
 * @param f Forest
 * @param threshold Weight cutoff
 * @param cluster_count Output: number of clusters
 * @return Array of cluster roots
 */
Node **get_clusters_by_weight(Forest *f, double threshold, int *cluster_count);

/**
 * Compute overlap coefficient between two nodes
 * Returns |intersection| / min(|A|, |B|)
 * 
 * @param a First node
 * @param b Second node
 * @return Overlap coefficient [0, 1]
 */
double compute_overlap(const Node *a, const Node *b);

// ========== UTILITY FUNCTIONS ==========

/**
 * Get total number of nodes in forest
 */
int count_total_nodes(Forest *f);

/**
 * Get maximum depth of any tree in forest
 */
int forest_max_depth(Forest *f);

/**
 * Get maximum weight in forest
 */
double forest_max_weight(Forest *f);

/**
 * Get minimum weight in forest
 */
double forest_min_weight(Forest *f);

/**
 * Print forest structure with weights
 */
void print_forest(Forest *f);

/**
 * Verify forest invariants:
 * - Weight monotonicity: parent.weight >= child.weight
 * - Subset preservation: if child ⊂ parent OR similar weight
 * 
 * @return 1 if valid, 0 if invalid
 */
int verify_forest(Forest *f);

/**
 * Get statistics about the forest
 */
typedef struct {
    int total_nodes;
    int num_roots;
    int max_depth;
    double max_weight;
    double min_weight;
    double avg_weight;
    int max_children;
} ForestStats;

ForestStats get_forest_stats(Forest *f);

#endif // HYPEREDGE_INCLUSION_FOREST_H
