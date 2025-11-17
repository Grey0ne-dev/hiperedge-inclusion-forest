/**
 * Comprehensive Test Suite for Hyperedge Inclusion Forest
 * Tests all advanced features: queries, optimization, serialization, traversal
 */

#include "../Core Implementation/hif.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define TEST_PASSED(name) printf("✓ %s PASSED\n", name)
#define TEST_FAILED(name) printf("✗ %s FAILED\n", name)

// ========== TEST 1: Advanced Query Operations ==========

void test_find_all_supersets() {
    printf("\n=== TEST 1: Find All Supersets ===\n");
    Forest *f = forest_create();
    
    int e1[] = {1,2,3,4,5};
    int e2[] = {1,2,3};
    int e3[] = {1,2};
    int e4[] = {1,2,3,4};
    int e5[] = {6,7};
    
    insert_hyperedge(f, e1, 5, 5.0);
    insert_hyperedge(f, e2, 3, 3.0);
    insert_hyperedge(f, e3, 2, 2.0);
    insert_hyperedge(f, e4, 4, 4.0);
    insert_hyperedge(f, e5, 2, 2.0);
    
    int query[] = {1,2};
    int count;
    Node **results = find_all_supersets(f, query, 2, &count);
    
    assert(count == 4);  // {1,2}, {1,2,3}, {1,2,3,4}, {1,2,3,4,5}
    free(results);
    forest_free(f);
    
    TEST_PASSED("find_all_supersets");
}

void test_find_all_subsets() {
    printf("\n=== TEST 2: Find All Subsets ===\n");
    Forest *f = forest_create();
    
    int e1[] = {1,2,3,4,5};
    int e2[] = {1,2,3};
    int e3[] = {1,2};
    int e4[] = {1};
    int e5[] = {6,7};
    
    insert_hyperedge(f, e1, 5, 5.0);
    insert_hyperedge(f, e2, 3, 3.0);
    insert_hyperedge(f, e3, 2, 2.0);
    insert_hyperedge(f, e4, 1, 1.0);
    insert_hyperedge(f, e5, 2, 2.0);
    
    int query[] = {1,2,3,4};
    int count;
    Node **results = find_all_subsets(f, query, 4, &count);
    
    assert(count >= 3);  // At least {1}, {1,2}, {1,2,3}
    free(results);
    forest_free(f);
    
    TEST_PASSED("find_all_subsets");
}

void test_find_by_weight_range() {
    printf("\n=== TEST 3: Find By Weight Range ===\n");
    Forest *f = forest_create();
    
    for (int i = 0; i < 20; i++) {
        int verts[] = {i, i+1};
        insert_hyperedge(f, verts, 2, (double)i);
    }
    
    int count;
    Node **results = find_by_weight_range(f, 5.0, 10.0, &count);
    
    assert(count == 6);  // weights 5,6,7,8,9,10
    free(results);
    forest_free(f);
    
    TEST_PASSED("find_by_weight_range");
}

void test_find_containing_vertices() {
    printf("\n=== TEST 4: Find Containing Vertices ===\n");
    Forest *f = forest_create();
    
    int e1[] = {1,2,3,4};
    int e2[] = {1,2,5};
    int e3[] = {1,2,6};
    int e4[] = {3,4,5};
    
    insert_hyperedge(f, e1, 4, 4.0);
    insert_hyperedge(f, e2, 3, 3.0);
    insert_hyperedge(f, e3, 3, 3.0);
    insert_hyperedge(f, e4, 3, 3.0);
    
    int query[] = {1,2};
    int count;
    Node **results = find_containing_vertices(f, query, 2, &count);
    
    assert(count == 3);  // e1, e2, e3 contain both 1 and 2
    free(results);
    forest_free(f);
    
    TEST_PASSED("find_containing_vertices");
}

void test_find_k_most_similar() {
    printf("\n=== TEST 5: Find K Most Similar ===\n");
    Forest *f = forest_create();
    
    int e1[] = {1,2,3};
    int e2[] = {1,2,4};
    int e3[] = {1,3,4};
    int e4[] = {5,6,7};
    
    insert_hyperedge(f, e1, 3, 3.0);
    insert_hyperedge(f, e2, 3, 3.0);
    insert_hyperedge(f, e3, 3, 3.0);
    insert_hyperedge(f, e4, 3, 3.0);
    
    int query[] = {1,2};
    int count;
    Node **results = find_k_most_similar(f, query, 2, 3, &count);
    
    assert(count == 3);
    // Most similar should be e1 and e2 (both have {1,2})
    
    free(results);
    forest_free(f);
    
    TEST_PASSED("find_k_most_similar");
}

// ========== TEST 2: Optimization & Maintenance ==========

void test_forest_rebalance() {
    printf("\n=== TEST 6: Forest Rebalance ===\n");
    Forest *f = forest_create();
    
    // Insert in suboptimal order
    for (int i = 0; i < 50; i++) {
        int verts[] = {i, i+1, i+2};
        insert_hyperedge(f, verts, 3, (double)(50-i));
    }
    
    int depth_before = forest_max_depth(f);
    forest_rebalance(f);
    int depth_after = forest_max_depth(f);
    
    printf("Depth before: %d, after: %d\n", depth_before, depth_after);
    forest_free(f);
    
    TEST_PASSED("forest_rebalance");
}

void test_merge_duplicates() {
    printf("\n=== TEST 7: Merge Duplicates ===\n");
    Forest *f = forest_create();
    
    int e[] = {1,2,3};
    insert_hyperedge(f, e, 3, 5.0);
    insert_hyperedge(f, e, 3, 7.0);
    insert_hyperedge(f, e, 3, 3.0);
    
    int total_before = count_total_nodes(f);
    int merged = forest_merge_duplicates(f, 1);  // keep max
    int total_after = count_total_nodes(f);
    
    printf("Merged %d duplicates\n", merged);
    printf("Nodes: %d → %d\n", total_before, total_after);
    assert(merged == 2);
    
    forest_free(f);
    TEST_PASSED("merge_duplicates");
}

void test_prune_by_weight() {
    printf("\n=== TEST 8: Prune By Weight ===\n");
    Forest *f = forest_create();
    
    for (int i = 0; i < 20; i++) {
        int verts[] = {i, i+1};
        insert_hyperedge(f, verts, 2, (double)i);
    }
    
    int total_before = count_total_nodes(f);
    int removed = forest_prune_by_weight(f, 10.0);
    int total_after = count_total_nodes(f);
    
    printf("Removed %d nodes below weight 10.0\n", removed);
    printf("Nodes: %d → %d\n", total_before, total_after);
    assert(removed >= 2);  // At least some nodes removed
    assert(total_after < total_before);
    
    forest_free(f);
    TEST_PASSED("prune_by_weight");
}

void test_forest_optimize() {
    printf("\n=== TEST 9: Forest Optimize ===\n");
    Forest *f = forest_create();
    
    for (int i = 0; i < 100; i++) {
        int verts[] = {i % 10, (i+1) % 10};
        insert_hyperedge(f, verts, 2, (double)(i % 20));
    }
    
    forest_optimize(f);
    
    printf("Optimization complete\n");
    forest_free(f);
    TEST_PASSED("forest_optimize");
}

// ========== TEST 3: Batch Operations ==========

void test_batch_insert() {
    printf("\n=== TEST 10: Batch Insert ===\n");
    Forest *f = forest_create();
    
    Hyperedge edges[10];
    for (int i = 0; i < 10; i++) {
        edges[i].verts = malloc(sizeof(int) * 3);
        edges[i].verts[0] = i;
        edges[i].verts[1] = i+1;
        edges[i].verts[2] = i+2;
        edges[i].nverts = 3;
        edges[i].weight = (double)i;
    }
    
    forest_insert_batch(f, edges, 10);
    
    int total = count_total_nodes(f);
    printf("Inserted 10 edges via batch, got %d nodes\n", total);
    assert(total == 10);
    
    for (int i = 0; i < 10; i++) {
        free(edges[i].verts);
    }
    forest_free(f);
    TEST_PASSED("batch_insert");
}

void test_bulk_build() {
    printf("\n=== TEST 11: Bulk Build ===\n");
    
    Hyperedge edges[20];
    for (int i = 0; i < 20; i++) {
        edges[i].verts = malloc(sizeof(int) * 2);
        edges[i].verts[0] = i;
        edges[i].verts[1] = i+1;
        edges[i].nverts = 2;
        edges[i].weight = (double)i;
    }
    
    Forest *f = forest_build_bulk(edges, 20);
    
    int total = count_total_nodes(f);
    printf("Bulk build created forest with %d nodes\n", total);
    assert(total == 20);
    
    for (int i = 0; i < 20; i++) {
        free(edges[i].verts);
    }
    forest_free(f);
    TEST_PASSED("bulk_build");
}

// ========== TEST 4: Serialization ==========

void test_serialization() {
    printf("\n=== TEST 12: Serialization ===\n");
    Forest *f = forest_create();
    
    for (int i = 0; i < 10; i++) {
        int verts[] = {i, i+1, i+2};
        insert_hyperedge(f, verts, 3, (double)i);
    }
    
    int total_before = count_total_nodes(f);
    
    // Save
    int save_result = forest_save(f, "/tmp/test_forest.bin");
    assert(save_result == 0);
    printf("Saved forest with %d nodes\n", total_before);
    
    forest_free(f);
    
    // Load
    Forest *f2 = forest_load("/tmp/test_forest.bin");
    assert(f2 != NULL);
    
    int total_after = count_total_nodes(f2);
    printf("Loaded forest with %d nodes\n", total_after);
    assert(total_before == total_after);
    
    forest_free(f2);
    remove("/tmp/test_forest.bin");
    TEST_PASSED("serialization");
}

// ========== TEST 5: Iteration ==========

static int visit_count = 0;
static int visit_sum = 0;

int test_visitor(Node *node, void *user_data) {
    visit_count++;
    visit_sum += node->he.nverts;
    return 0;
}

int early_stop_visitor(Node *node, void *user_data) {
    visit_count++;
    return (visit_count >= 5) ? 1 : 0;  // Stop after 5 nodes
}

void test_traverse_bfs() {
    printf("\n=== TEST 13: BFS Traversal ===\n");
    Forest *f = forest_create();
    
    for (int i = 0; i < 10; i++) {
        int verts[] = {i, i+1};
        insert_hyperedge(f, verts, 2, (double)i);
    }
    
    visit_count = 0;
    visit_sum = 0;
    forest_traverse_bfs(f, test_visitor, NULL);
    
    printf("BFS visited %d nodes, sum of nverts = %d\n", visit_count, visit_sum);
    assert(visit_count == 10);
    
    forest_free(f);
    TEST_PASSED("traverse_bfs");
}

void test_traverse_dfs() {
    printf("\n=== TEST 14: DFS Traversal ===\n");
    Forest *f = forest_create();
    
    for (int i = 0; i < 10; i++) {
        int verts[] = {i, i+1};
        insert_hyperedge(f, verts, 2, (double)i);
    }
    
    visit_count = 0;
    visit_sum = 0;
    forest_traverse_dfs(f, test_visitor, NULL);
    
    printf("DFS visited %d nodes, sum of nverts = %d\n", visit_count, visit_sum);
    assert(visit_count == 10);
    
    forest_free(f);
    TEST_PASSED("traverse_dfs");
}

void test_traverse_by_weight() {
    printf("\n=== TEST 15: Weight-Order Traversal ===\n");
    Forest *f = forest_create();
    
    for (int i = 0; i < 10; i++) {
        int verts[] = {i, i+1};
        insert_hyperedge(f, verts, 2, (double)i);
    }
    
    visit_count = 0;
    forest_traverse_by_weight(f, test_visitor, NULL);
    
    printf("Weight-order visited %d nodes\n", visit_count);
    assert(visit_count == 10);
    
    forest_free(f);
    TEST_PASSED("traverse_by_weight");
}

void test_early_stop() {
    printf("\n=== TEST 16: Early Stop Traversal ===\n");
    Forest *f = forest_create();
    
    for (int i = 0; i < 20; i++) {
        int verts[] = {i, i+1};
        insert_hyperedge(f, verts, 2, (double)i);
    }
    
    visit_count = 0;
    forest_traverse_bfs(f, early_stop_visitor, NULL);
    
    printf("Early stop after %d nodes (total: %d)\n", visit_count, count_total_nodes(f));
    assert(visit_count == 5);
    
    forest_free(f);
    TEST_PASSED("early_stop");
}

// ========== TEST 6: Top-K Performance ==========

void test_top_k_performance() {
    printf("\n=== TEST 17: Top-K Query Performance ===\n");
    Forest *f = forest_create();
    
    // Insert 1000 hyperedges
    for (int i = 0; i < 1000; i++) {
        int verts[] = {i, i+1, i+2};
        insert_hyperedge(f, verts, 3, (double)(1000-i));
    }
    
    // Query top-10
    int count;
    Node **results = find_top_k(f, 10, &count);
    
    assert(count == 10);
    
    // Verify they are indeed the top 10 by weight
    for (int i = 0; i < 10; i++) {
        printf("Top-%d: weight=%.1f\n", i+1, results[i]->he.weight);
    }
    
    assert(results[0]->he.weight == 1000.0);
    assert(results[9]->he.weight == 991.0);
    
    free(results);
    forest_free(f);
    TEST_PASSED("top_k_performance");
}

// ========== MAIN ==========

int main(void) {
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║   COMPREHENSIVE TEST SUITE - ADVANCED FEATURES      ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    
    // Advanced Queries
    test_find_all_supersets();
    test_find_all_subsets();
    test_find_by_weight_range();
    test_find_containing_vertices();
    test_find_k_most_similar();
    
    // Optimization
    test_forest_rebalance();
    test_merge_duplicates();
    test_prune_by_weight();
    test_forest_optimize();
    
    // Batch Operations
    test_batch_insert();
    test_bulk_build();
    
    // Serialization
    test_serialization();
    
    // Iteration
    test_traverse_bfs();
    test_traverse_dfs();
    test_traverse_by_weight();
    test_early_stop();
    
    // Performance
    test_top_k_performance();
    
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║            ALL 17 TESTS PASSED ✓✓✓                  ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");
    
    printf("Summary of tested features:\n");
    printf("✓ Advanced query operations (5 tests)\n");
    printf("✓ Optimization & maintenance (4 tests)\n");
    printf("✓ Batch operations (2 tests)\n");
    printf("✓ Serialization (1 test)\n");
    printf("✓ Traversal & iteration (4 tests)\n");
    printf("✓ Top-K performance (1 test)\n\n");
    
    return 0;
}
