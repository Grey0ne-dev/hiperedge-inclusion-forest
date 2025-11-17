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

// ========== ADVANCED QUERY OPERATIONS ==========

/**
 * Find all supersets of a query set
 * Returns array of nodes (caller must free)
 * 
 * @param f Forest to search
 * @param query Query vertex set
 * @param nquery Number of vertices
 * @param result_count Output: number of results
 * @return Array of superset nodes
 */
Node **find_all_supersets(Forest *f, const int *query, int nquery, int *result_count);

/**
 * Find all subsets of a query set
 * 
 * @param f Forest to search
 * @param query Query vertex set
 * @param nquery Number of vertices
 * @param result_count Output: number of results
 * @return Array of subset nodes
 */
Node **find_all_subsets(Forest *f, const int *query, int nquery, int *result_count);

/**
 * Find nodes with weight in range [min_weight, max_weight]
 * 
 * @param f Forest to search
 * @param min_weight Minimum weight (inclusive)
 * @param max_weight Maximum weight (inclusive)
 * @param result_count Output: number of results
 * @return Array of nodes in weight range
 */
Node **find_by_weight_range(Forest *f, double min_weight, double max_weight, int *result_count);

/**
 * Find nodes containing specific vertices
 * 
 * @param f Forest to search
 * @param vertices Array of required vertices
 * @param nvertices Number of required vertices
 * @param result_count Output: number of results
 * @return Array of nodes containing all specified vertices
 */
Node **find_containing_vertices(Forest *f, const int *vertices, int nvertices, int *result_count);

/**
 * Find the k most similar nodes to a query set
 * Similarity measured by overlap coefficient
 * 
 * @param f Forest to search
 * @param query Query vertex set
 * @param nquery Number of vertices
 * @param k Number of results to return
 * @param result_count Output: actual number returned
 * @return Array of k most similar nodes
 */
Node **find_k_most_similar(Forest *f, const int *query, int nquery, int k, int *result_count);

// ========== OPTIMIZATION & MAINTENANCE ==========

/**
 * Rebalance the forest to improve query performance
 * Reorganizes nodes to reduce average depth
 * 
 * @param f Forest to rebalance
 */
void forest_rebalance(Forest *f);

/**
 * Merge duplicate hyperedges (same vertex set, different weights)
 * Strategy: keep maximum weight or average weights
 * 
 * @param f Forest to deduplicate
 * @param keep_max 1 = keep max weight, 0 = average weights
 * @return Number of duplicates merged
 */
int forest_merge_duplicates(Forest *f, int keep_max);

/**
 * Prune nodes below weight threshold
 * 
 * @param f Forest to prune
 * @param threshold Minimum weight to keep
 * @return Number of nodes removed
 */
int forest_prune_by_weight(Forest *f, double threshold);

/**
 * Optimize forest for query performance
 * Combines rebalancing and internal optimization
 * 
 * @param f Forest to optimize
 */
void forest_optimize(Forest *f);

// ========== BATCH OPERATIONS ==========

/**
 * Insert multiple hyperedges efficiently
 * More efficient than individual inserts
 * 
 * @param f Forest to insert into
 * @param edges Array of hyperedges
 * @param nedges Number of hyperedges
 */
void forest_insert_batch(Forest *f, Hyperedge *edges, int nedges);

/**
 * Build forest from array of hyperedges
 * More efficient than incremental insertion
 * 
 * @param edges Array of hyperedges
 * @param nedges Number of hyperedges
 * @return New forest containing all edges
 */
Forest *forest_build_bulk(Hyperedge *edges, int nedges);

// ========== SERIALIZATION ==========

/**
 * Save forest to file
 * 
 * @param f Forest to save
 * @param filename Output filename
 * @return 0 on success, -1 on error
 */
int forest_save(Forest *f, const char *filename);

/**
 * Load forest from file
 * 
 * @param filename Input filename
 * @return Loaded forest, or NULL on error
 */
Forest *forest_load(const char *filename);

// ========== ITERATION ==========

/**
 * Callback function for forest traversal
 * Return 0 to continue, non-zero to stop
 */
typedef int (*NodeVisitor)(Node *node, void *user_data);

/**
 * Traverse forest in breadth-first order
 * 
 * @param f Forest to traverse
 * @param visitor Callback function
 * @param user_data User data passed to callback
 */
void forest_traverse_bfs(Forest *f, NodeVisitor visitor, void *user_data);

/**
 * Traverse forest in depth-first order
 * 
 * @param f Forest to traverse
 * @param visitor Callback function
 * @param user_data User data passed to callback
 */
void forest_traverse_dfs(Forest *f, NodeVisitor visitor, void *user_data);

/**
 * Traverse forest by weight order (heaviest first)
 * 
 * @param f Forest to traverse
 * @param visitor Callback function
 * @param user_data User data passed to callback
 */
void forest_traverse_by_weight(Forest *f, NodeVisitor visitor, void *user_data);

#endif // HYPEREDGE_INCLUSION_FOREST_H
