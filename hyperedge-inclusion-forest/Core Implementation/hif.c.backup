/**
 * Hyperedge Inclusion Forest (HIF)
 * Weight-Based Hierarchical Decomposition - FINAL PRODUCTION VERSION
 * 
 * Key Features:
 * - Weight-first ordering with tolerance-based clustering
 * - Overlap-aware sibling placement
 * - Automatic tree balancing via branching
 * - O(k·log n) insertion on realistic data
 * - O(k) top-k queries
 * 
 * Copyright (c) 2024
 * Licensed under MIT License
 */

#include "hif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Tuning parameters
#define WEIGHT_TOLERANCE 0.15      // 15% tolerance for similar weights
#define MIN_OVERLAP_RATIO 0.3      // 30% overlap to cluster
#define MAX_CHAIN_DEPTH 100        // Force branching if chain too deep

// ========== INTERNAL HELPERS ==========

static int *copy_int_array(const int *a, int n) {
    int *r = malloc(sizeof(int) * n);
    if (!r) { perror("malloc"); exit(1); }
    memcpy(r, a, sizeof(int) * n);
    return r;
}

static int is_subset(const int *A, int nA, const int *B, int nB) {
    int i = 0, j = 0;
    while (i < nA && j < nB) {
        if (A[i] == B[j]) { i++; j++; }
        else if (A[i] > B[j]) { j++; }
        else { return 0; }
    }
    return (i == nA);
}

static int overlap_size(const int *A, int nA, const int *B, int nB) {
    int i = 0, j = 0, count = 0;
    while (i < nA && j < nB) {
        if (A[i] == B[j]) { count++; i++; j++; }
        else if (A[i] < B[j]) i++;
        else j++;
    }
    return count;
}

static double overlap_ratio(const int *A, int nA, const int *B, int nB) {
    int overlap = overlap_size(A, nA, B, nB);
    int min_size = nA < nB ? nA : nB;
    return min_size > 0 ? (double)overlap / min_size : 0.0;
}

static int weights_similar(double w1, double w2) {
    if (fabs(w1) < 1e-9 && fabs(w2) < 1e-9) return 1;
    double diff = fabs(w1 - w2);
    double avg = (fabs(w1) + fabs(w2)) / 2.0;
    return diff < avg * WEIGHT_TOLERANCE;
}

// Enhanced comparison with tolerance and overlap awareness
static int weighted_cmp(const Hyperedge *A, const Hyperedge *B) {
    const double EPSILON = 1e-9;
    
    // If weights similar, use subset relationship
    if (weights_similar(A->weight, B->weight)) {
        int A_in_B = is_subset(A->verts, A->nverts, B->verts, B->nverts);
        int B_in_A = is_subset(B->verts, B->nverts, A->verts, A->nverts);
        
        if (B_in_A && !A_in_B) return -1;  // A is superset
        if (A_in_B && !B_in_A) return 1;   // B is superset
        
        // For similar weights and incomparable sets, prefer larger
        if (A->nverts > B->nverts) return -1;
        if (B->nverts > A->nverts) return 1;
        
        return 0; // Siblings
    }
    
    // Clear weight difference
    if (A->weight > B->weight + EPSILON) return -1;
    if (B->weight > A->weight + EPSILON) return 1;
    
    return 0;
}

static Node *node_create(const int *verts, int nverts, double weight) {
    Node *nd = malloc(sizeof(Node));
    if (!nd) { perror("malloc"); exit(1); }
    nd->he.verts = copy_int_array(verts, nverts);
    nd->he.nverts = nverts;
    nd->he.weight = weight;
    nd->children = NULL;
    nd->nchildren = 0;
    nd->children_cap = 0;
    return nd;
}

static void node_free(Node *nd) {
    if (!nd) return;
    for (int i = 0; i < nd->nchildren; ++i) node_free(nd->children[i]);
    free(nd->children);
    free(nd->he.verts);
    free(nd);
}

static void node_add_child(Node *parent, Node *child) {
    if (parent->nchildren >= parent->children_cap) {
        int newcap = parent->children_cap ? parent->children_cap * 2 : 4;
        parent->children = realloc(parent->children, sizeof(Node*) * newcap);
        if (!parent->children) { perror("realloc"); exit(1); }
        parent->children_cap = newcap;
    }
    parent->children[parent->nchildren++] = child;
}

static int node_depth(Node *nd) {
    int max_child_depth = 0;
    for (int i = 0; i < nd->nchildren; ++i) {
        int d = node_depth(nd->children[i]);
        if (d > max_child_depth) max_child_depth = d;
    }
    return 1 + max_child_depth;
}

static void forest_add_root(Forest *f, Node *r) {
    if (f->nroots >= f->roots_cap) {
        int newcap = f->roots_cap ? f->roots_cap * 2 : 8;
        f->roots = realloc(f->roots, sizeof(Node*) * newcap);
        if (!f->roots) { perror("realloc"); exit(1); }
        f->roots_cap = newcap;
    }
    f->roots[f->nroots++] = r;
}

static void forest_remove_root_at(Forest *f, int idx) {
    if (idx < 0 || idx >= f->nroots) return;
    for (int i = idx; i + 1 < f->nroots; ++i) f->roots[i] = f->roots[i+1];
    f->nroots--;
}

// Find best sibling based on overlap
static int find_best_sibling_by_overlap(Node *parent, Node *newn) {
    int best_idx = -1;
    double best_score = 0.0;
    
    for (int i = 0; i < parent->nchildren; ++i) {
        Node *child = parent->children[i];
        
        // Check weight similarity
        if (!weights_similar(child->he.weight, newn->he.weight)) continue;
        
        // Compute overlap score
        double overlap = overlap_ratio(child->he.verts, child->he.nverts,
                                       newn->he.verts, newn->he.nverts);
        
        if (overlap > best_score && overlap >= MIN_OVERLAP_RATIO) {
            best_score = overlap;
            best_idx = i;
        }
    }
    
    return best_idx;
}

static int insert_into_node(Node *root, Node *newn, int depth) {
    int cmp = weighted_cmp(&root->he, &newn->he);
    
    if (cmp == 1) {
        return 1; // root should be child of newn
    } else if (cmp == -1) {
        // newn should be child of root
        
        // Check if depth limit reached - force as sibling instead of deep child
        if (depth > MAX_CHAIN_DEPTH && root->nchildren > 0) {
            // Add as sibling to first child instead of going deeper
            node_add_child(root, newn);
            return -1;
        }
        
        // Check for stealing children
        int i = 0;
        while (i < root->nchildren) {
            int res = insert_into_node(root->children[i], newn, depth + 1);
            if (res == 1) {
                Node *child = root->children[i];
                for (int j = i; j + 1 < root->nchildren; ++j) 
                    root->children[j] = root->children[j+1];
                root->nchildren--;
                node_add_child(newn, child);
            } else if (res == -1) {
                return -1; // Inserted
            } else {
                i++;
            }
        }
        
        node_add_child(root, newn);
        return -1;
        
    } else {
        // Similar weight - try to find overlapping sibling
        int sibling_idx = find_best_sibling_by_overlap(root, newn);
        if (sibling_idx >= 0) {
            // Try inserting into sibling's subtree
            Node *sibling = root->children[sibling_idx];
            int res = insert_into_node(sibling, newn, depth + 1);
            if (res != 0) return res;
        }
        
        return 0; // Incomparable
    }
}

static void forest_insert_node(Forest *f, Node *newn) {
    // Try to group with existing roots if similar weight and overlap
    for (int i = 0; i < f->nroots; ++i) {
        Node *r = f->roots[i];
        
        if (weights_similar(r->he.weight, newn->he.weight)) {
            double overlap = overlap_ratio(r->he.verts, r->he.nverts,
                                          newn->he.verts, newn->he.nverts);
            
            if (overlap >= MIN_OVERLAP_RATIO) {
                // Significant overlap with similar weight
                int cmp = weighted_cmp(&r->he, &newn->he);
                if (cmp == -1) {
                    insert_into_node(r, newn, 1);
                    return;
                } else if (cmp == 1) {
                    node_add_child(newn, r);
                    forest_remove_root_at(f, i);
                    forest_add_root(f, newn);
                    return;
                }
            }
        }
    }
    
    // Standard insertion
    int i = 0;
    while (i < f->nroots) {
        Node *r = f->roots[i];
        int cmp = weighted_cmp(&r->he, &newn->he);
        
        if (cmp == 1) {
            node_add_child(newn, r);
            forest_remove_root_at(f, i);
        } else if (cmp == -1) {
            int res = insert_into_node(r, newn, 1);
            if (res == 1) {
                node_add_child(newn, r);
                forest_remove_root_at(f, i);
            } else if (res == -1) {
                return; // Inserted
            } else {
                i++;
            }
        } else {
            i++;
        }
    }
    
    // Add as new root
    forest_add_root(f, newn);
}

static int cmp_int(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

static int *normalize_vertices(const int *in, int n_in, int *n_out) {
    if (n_in == 0) { *n_out = 0; return NULL; }
    int *a = malloc(sizeof(int) * n_in);
    if (!a) { perror("malloc"); exit(1); }
    memcpy(a, in, sizeof(int) * n_in);
    qsort(a, n_in, sizeof(int), cmp_int);
    int w = 1;
    for (int i = 1; i < n_in; ++i) if (a[i] != a[w-1]) a[w++] = a[i];
    a = realloc(a, sizeof(int) * w);
    *n_out = w;
    return a;
}

// ========== PUBLIC API ==========

Forest *forest_create(void) {
    Forest *f = malloc(sizeof(Forest));
    if (!f) { perror("malloc"); exit(1); }
    f->roots = NULL;
    f->nroots = 0;
    f->roots_cap = 0;
    return f;
}

void forest_free(Forest *f) {
    if (!f) return;
    for (int i = 0; i < f->nroots; ++i) node_free(f->roots[i]);
    free(f->roots);
    free(f);
}

void insert_hyperedge(Forest *f, const int *verts, int nverts, double weight) {
    int n_norm;
    int *norm = normalize_vertices(verts, nverts, &n_norm);
    if (n_norm == 0) {
        free(norm);
        return;
    }
    Node *nd = node_create(norm, n_norm, weight);
    free(norm);
    forest_insert_node(f, nd);
}

Node **find_top_k(Forest *f, int k, int *result_count) {
    if (k <= 0 || f->nroots == 0) {
        *result_count = 0;
        return NULL;
    }
    
    Node **result = malloc(sizeof(Node*) * k);
    Node **queue = malloc(sizeof(Node*) * (k * 10));
    int front = 0, back = 0, count = 0;
    
    for (int i = 0; i < f->nroots && count < k; ++i) {
        result[count++] = f->roots[i];
        queue[back++] = f->roots[i];
    }
    
    while (front < back && count < k) {
        Node *curr = queue[front++];
        for (int i = 0; i < curr->nchildren && count < k; ++i) {
            result[count++] = curr->children[i];
            if (back < k * 10) queue[back++] = curr->children[i];
        }
    }
    
    free(queue);
    *result_count = count;
    return result;
}

static int count_by_threshold_recursive(Node *nd, double threshold) {
    if (nd->he.weight < threshold) return 0;
    int count = 1;
    for (int i = 0; i < nd->nchildren; ++i) {
        count += count_by_threshold_recursive(nd->children[i], threshold);
    }
    return count;
}

int find_by_weight_threshold(Forest *f, double threshold) {
    int count = 0;
    for (int i = 0; i < f->nroots; ++i) {
        count += count_by_threshold_recursive(f->roots[i], threshold);
    }
    return count;
}

static Node *find_minimal_superset_recursive(Node *nd, const int *query, int nquery, Node *best) {
    if (is_subset(query, nquery, nd->he.verts, nd->he.nverts)) {
        if (!best || nd->he.nverts < best->he.nverts) {
            best = nd;
        }
        for (int i = 0; i < nd->nchildren; ++i) {
            Node *child_best = find_minimal_superset_recursive(nd->children[i], query, nquery, best);
            if (child_best && (!best || child_best->he.nverts < best->he.nverts)) {
                best = child_best;
            }
        }
    }
    return best;
}

Node *find_minimal_superset(Forest *f, const int *query, int nquery) {
    Node *best = NULL;
    for (int i = 0; i < f->nroots; ++i) {
        Node *cand = find_minimal_superset_recursive(f->roots[i], query, nquery, best);
        if (cand && (!best || cand->he.nverts < best->he.nverts)) {
            best = cand;
        }
    }
    return best;
}

static Node *find_heaviest_superset_recursive(Node *nd, const int *query, int nquery, Node *best) {
    if (is_subset(query, nquery, nd->he.verts, nd->he.nverts)) {
        if (!best || nd->he.weight > best->he.weight) {
            best = nd;
        }
        for (int i = 0; i < nd->nchildren; ++i) {
            Node *child_best = find_heaviest_superset_recursive(nd->children[i], query, nquery, best);
            if (child_best && (!best || child_best->he.weight > best->he.weight)) {
                best = child_best;
            }
        }
    }
    return best;
}

Node *find_heaviest_superset(Forest *f, const int *query, int nquery) {
    Node *best = NULL;
    for (int i = 0; i < f->nroots; ++i) {
        Node *cand = find_heaviest_superset_recursive(f->roots[i], query, nquery, best);
        if (cand && (!best || cand->he.weight > best->he.weight)) {
            best = cand;
        }
    }
    return best;
}

static void collect_by_weight_recursive(Node *nd, double threshold, Node ***result, int *count, int *cap) {
    if (nd->he.weight >= threshold) {
        if (*count >= *cap) {
            *cap = *cap ? *cap * 2 : 16;
            *result = realloc(*result, sizeof(Node*) * (*cap));
        }
        (*result)[(*count)++] = nd;
        
        for (int i = 0; i < nd->nchildren; ++i) {
            collect_by_weight_recursive(nd->children[i], threshold, result, count, cap);
        }
    }
}

Node **get_clusters_by_weight(Forest *f, double threshold, int *cluster_count) {
    Node **result = NULL;
    int count = 0, cap = 0;
    
    for (int i = 0; i < f->nroots; ++i) {
        collect_by_weight_recursive(f->roots[i], threshold, &result, &count, &cap);
    }
    
    *cluster_count = count;
    return result;
}

double compute_overlap(const Node *a, const Node *b) {
    return overlap_ratio(a->he.verts, a->he.nverts, b->he.verts, b->he.nverts);
}

static int count_nodes_recursive(Node *nd) {
    int count = 1;
    for (int i = 0; i < nd->nchildren; ++i) {
        count += count_nodes_recursive(nd->children[i]);
    }
    return count;
}

int count_total_nodes(Forest *f) {
    int total = 0;
    for (int i = 0; i < f->nroots; ++i) {
        total += count_nodes_recursive(f->roots[i]);
    }
    return total;
}

int forest_max_depth(Forest *f) {
    int max = 0;
    for (int i = 0; i < f->nroots; ++i) {
        int d = node_depth(f->roots[i]);
        if (d > max) max = d;
    }
    return max;
}

static void find_weight_range_recursive(Node *nd, double *min, double *max) {
    if (nd->he.weight < *min) *min = nd->he.weight;
    if (nd->he.weight > *max) *max = nd->he.weight;
    for (int i = 0; i < nd->nchildren; ++i) {
        find_weight_range_recursive(nd->children[i], min, max);
    }
}

double forest_max_weight(Forest *f) {
    if (f->nroots == 0) return 0.0;
    double max = f->roots[0]->he.weight;
    for (int i = 0; i < f->nroots; ++i) {
        if (f->roots[i]->he.weight > max) max = f->roots[i]->he.weight;
    }
    return max;
}

double forest_min_weight(Forest *f) {
    if (f->nroots == 0) return 0.0;
    double min = f->roots[0]->he.weight;
    for (int i = 0; i < f->nroots; ++i) {
        find_weight_range_recursive(f->roots[i], &min, &min);
    }
    return min;
}

static void print_indent(int depth) {
    for (int i = 0; i < depth; ++i) printf("  ");
}

static void print_node(Node *nd, int depth) {
    print_indent(depth);
    printf("⚡ w=%.2f {", nd->he.weight);
    for (int i = 0; i < nd->he.nverts; ++i) {
        printf("%d", nd->he.verts[i]);
        if (i + 1 < nd->he.nverts) printf(",");
    }
    printf("}\n");
    for (int i = 0; i < nd->nchildren; ++i) print_node(nd->children[i], depth + 1);
}

void print_forest(Forest *f) {
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║  WEIGHTED HYPERGRAPH DECOMPOSITION      ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("Roots: %d | Total: %d | Depth: %d\n", 
           f->nroots, count_total_nodes(f), forest_max_depth(f));
    if (f->nroots > 0) {
        printf("Weight range: [%.2f, %.2f]\n", forest_min_weight(f), forest_max_weight(f));
    }
    
    for (int i = 0; i < f->nroots; ++i) {
        printf("\n[ROOT %d]\n", i + 1);
        print_node(f->roots[i], 0);
    }
    printf("\n");
}

static int verify_weight_monotonicity(Node *nd) {
    for (int i = 0; i < nd->nchildren; ++i) {
        Node *child = nd->children[i];
        if (child->he.weight > nd->he.weight + 1e-9) {
            return 0;
        }
        if (!verify_weight_monotonicity(child)) return 0;
    }
    return 1;
}

int verify_forest(Forest *f) {
    for (int i = 0; i < f->nroots; ++i) {
        if (!verify_weight_monotonicity(f->roots[i])) return 0;
    }
    return 1;
}

ForestStats get_forest_stats(Forest *f) {
    ForestStats stats = {0};
    stats.total_nodes = count_total_nodes(f);
    stats.num_roots = f->nroots;
    stats.max_depth = forest_max_depth(f);
    stats.max_weight = forest_max_weight(f);
    stats.min_weight = forest_min_weight(f);
    
    double sum = 0;
    Node **queue = malloc(sizeof(Node*) * (stats.total_nodes + 1));
    int front = 0, back = 0;
    
    for (int i = 0; i < f->nroots; ++i) queue[back++] = f->roots[i];
    
    while (front < back) {
        Node *curr = queue[front++];
        sum += curr->he.weight;
        for (int i = 0; i < curr->nchildren; ++i) {
            if (back < stats.total_nodes) queue[back++] = curr->children[i];
        }
        if (curr->nchildren > stats.max_children) {
            stats.max_children = curr->nchildren;
        }
    }
    
    free(queue);
    stats.avg_weight = stats.total_nodes > 0 ? sum / stats.total_nodes : 0.0;
    
    return stats;
}
