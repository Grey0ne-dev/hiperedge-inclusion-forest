/**
 * Hyperedge Inclusion Forest (HIF)
 * Weight-Based Hierarchical Decomposition
 *
 * Changes from initial version:
 *   - Added NodeHeap (max-heap by weight) embedded in Forest
 *   - find_top_k: now truly O(k log k) via working heap expansion
 *   - forest_insert_batch: sorts by weight desc before inserting
 *   - forest_merge_duplicates: fixed averaging bug (divide-once-at-end)
 *   - forest_rebalance: frees children[] pointers before rebuild
 *   - get_forest_stats: fixed BFS queue off-by-one bound
 *   - forest_max_weight: O(1) via root_heap peek
 *
 * Copyright (c) 2024
 * Licensed under MIT License
 */

#include "hif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========== TUNING PARAMETERS ========== */

#define WEIGHT_TOLERANCE  0.15   /* 15% tolerance for "similar" weights  */
#define MIN_OVERLAP_RATIO 0.30   /* 30% overlap required to cluster      */
#define MAX_CHAIN_DEPTH   100    /* force branching beyond this depth    */

/* ========== INTERNAL HELPERS ========== */

static int *copy_int_array(const int *a, int n)
{
    int *r = malloc(sizeof(int) * n);
    if (!r) { perror("malloc"); exit(1); }
    memcpy(r, a, sizeof(int) * n);
    return r;
}

/* Returns 1 if sorted array A is a subset of sorted array B. */
static int is_subset(const int *A, int nA, const int *B, int nB)
{
    int i = 0, j = 0;
    while (i < nA && j < nB) {
        if      (A[i] == B[j]) { i++; j++; }
        else if (A[i] >  B[j]) { j++; }
        else                    { return 0; }
    }
    return (i == nA);
}

static int overlap_size(const int *A, int nA, const int *B, int nB)
{
    int i = 0, j = 0, count = 0;
    while (i < nA && j < nB) {
        if      (A[i] == B[j]) { count++; i++; j++; }
        else if (A[i] <  B[j]) { i++; }
        else                    { j++; }
    }
    return count;
}

static double overlap_ratio(const int *A, int nA, const int *B, int nB)
{
    int ov  = overlap_size(A, nA, B, nB);
    int mn  = nA < nB ? nA : nB;
    return mn > 0 ? (double)ov / mn : 0.0;
}

static int weights_similar(double w1, double w2)
{
    if (fabs(w1) < 1e-9 && fabs(w2) < 1e-9) return 1;
    double diff = fabs(w1 - w2);
    double avg  = (fabs(w1) + fabs(w2)) / 2.0;
    return diff < avg * WEIGHT_TOLERANCE;
}

/*
 * Strict weight-first comparison.
 * Returns -1 if A should be parent, +1 if B should be parent, 0 if siblings.
 */
static int weighted_cmp(const Hyperedge *A, const Hyperedge *B)
{
    const double EPS = 1e-9;

    if (A->weight > B->weight + EPS) return -1;
    if (B->weight > A->weight + EPS) return  1;

    /* Weights equal: use subset relationship */
    int A_in_B = is_subset(A->verts, A->nverts, B->verts, B->nverts);
    int B_in_A = is_subset(B->verts, B->nverts, A->verts, A->nverts);

    if (B_in_A && !A_in_B) return -1;
    if (A_in_B && !B_in_A) return  1;

    /* Equal weight, incomparable sets: prefer larger for stability */
    if (A->nverts > B->nverts) return -1;
    if (B->nverts > A->nverts) return  1;

    return 0; /* complete tie → siblings */
}

/* ========== NODE HELPERS ========== */

static Node *node_create(const int *verts, int nverts, double weight)
{
    Node *nd = malloc(sizeof(Node));
    if (!nd) { perror("malloc"); exit(1); }
    nd->he.verts    = copy_int_array(verts, nverts);
    nd->he.nverts   = nverts;
    nd->he.weight   = weight;
    nd->children    = NULL;
    nd->nchildren   = 0;
    nd->children_cap = 0;
    return nd;
}

static void node_free(Node *nd)
{
    if (!nd) return;
    for (int i = 0; i < nd->nchildren; ++i) node_free(nd->children[i]);
    free(nd->children);
    free(nd->he.verts);
    free(nd);
}

static void node_add_child(Node *parent, Node *child)
{
    if (parent->nchildren >= parent->children_cap) {
        int newcap = parent->children_cap ? parent->children_cap * 2 : 4;
        parent->children = realloc(parent->children, sizeof(Node*) * newcap);
        if (!parent->children) { perror("realloc"); exit(1); }
        parent->children_cap = newcap;
    }
    parent->children[parent->nchildren++] = child;
}

static int node_depth(Node *nd)
{
    int mx = 0;
    for (int i = 0; i < nd->nchildren; ++i) {
        int d = node_depth(nd->children[i]);
        if (d > mx) mx = d;
    }
    return 1 + mx;
}

/* ========== HEAP IMPLEMENTATION ========== */

NodeHeap *heap_create(void)
{
    NodeHeap *h = malloc(sizeof(NodeHeap));
    if (!h) { perror("malloc"); exit(1); }
    h->data = NULL;
    h->size = 0;
    h->cap  = 0;
    return h;
}

void heap_free(NodeHeap *h)
{
    if (!h) return;
    free(h->data);
    free(h);
}

static void heap_ensure_cap(NodeHeap *h)
{
    if (h->size < h->cap) return;
    h->cap  = h->cap ? h->cap * 2 : 8;
    h->data = realloc(h->data, sizeof(Node*) * h->cap);
    if (!h->data) { perror("realloc"); exit(1); }
}

static void heap_sift_up(NodeHeap *h, int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->data[parent]->he.weight >= h->data[i]->he.weight) break;
        Node *tmp        = h->data[parent];
        h->data[parent]  = h->data[i];
        h->data[i]       = tmp;
        i = parent;
    }
}

static void heap_sift_down(NodeHeap *h, int i)
{
    while (1) {
        int largest = i;
        int left    = 2 * i + 1;
        int right   = 2 * i + 2;
        if (left  < h->size && h->data[left]->he.weight  > h->data[largest]->he.weight) largest = left;
        if (right < h->size && h->data[right]->he.weight > h->data[largest]->he.weight) largest = right;
        if (largest == i) break;
        Node *tmp        = h->data[i];
        h->data[i]       = h->data[largest];
        h->data[largest] = tmp;
        i = largest;
    }
}

void heap_push(NodeHeap *h, Node *nd)
{
    heap_ensure_cap(h);
    h->data[h->size++] = nd;
    heap_sift_up(h, h->size - 1);
}

Node *heap_pop(NodeHeap *h)
{
    if (h->size == 0) return NULL;
    Node *top    = h->data[0];
    h->data[0]   = h->data[--h->size];
    if (h->size > 0) heap_sift_down(h, 0);
    return top;
}

/* ========== FOREST ROOT MANAGEMENT ========== */

static void forest_rebuild_root_heap(Forest *f)
{
    f->root_heap->size = 0;
    for (int i = 0; i < f->nroots; ++i)
        heap_push(f->root_heap, f->roots[i]);
}

static void forest_add_root(Forest *f, Node *r)
{
    if (f->nroots >= f->roots_cap) {
        int newcap  = f->roots_cap ? f->roots_cap * 2 : 8;
        f->roots    = realloc(f->roots, sizeof(Node*) * newcap);
        if (!f->roots) { perror("realloc"); exit(1); }
        f->roots_cap = newcap;
    }
    f->roots[f->nroots++] = r;
    heap_push(f->root_heap, r);
}

static void forest_remove_root_at(Forest *f, int idx)
{
    if (idx < 0 || idx >= f->nroots) return;
    for (int i = idx; i + 1 < f->nroots; ++i) f->roots[i] = f->roots[i+1];
    f->nroots--;
    forest_rebuild_root_heap(f);
}

/* ========== INSERTION INTERNALS ========== */

/*
 * Try to insert newn into the subtree rooted at `root`.
 *
 * Returns:
 *   -1  successfully placed newn somewhere under root
 *    0  newn is incomparable with root (try next sibling)
 *    1  root should become a child of newn (caller handles steal)
 */
static int insert_into_node(Node *root, Node *newn, int depth)
{
    int cmp = weighted_cmp(&root->he, &newn->he);

    if (cmp == 1) {
        /* root is lighter → should move under newn */
        return 1;
    }

    if (cmp == -1) {
        /* root is heavier → newn could go under root,
           but only if newn ⊆ root (avoid fake hierarchy) */
        int newn_in_root = is_subset(newn->he.verts, newn->he.nverts,
                                     root->he.verts, root->he.nverts);
        if (!newn_in_root) return 0; /* incomparable sets */

        /* Try to place newn deeper in root's children */
        int i = 0;
        while (i < root->nchildren) {
            int res = insert_into_node(root->children[i], newn, depth + 1);
            if (res == 1) {
                /* steal: child moves under newn */
                Node *child = root->children[i];
                for (int j = i; j + 1 < root->nchildren; ++j)
                    root->children[j] = root->children[j+1];
                root->nchildren--;
                node_add_child(newn, child);
                /* don't advance i; check same slot again */
            } else if (res == -1) {
                return -1; /* placed successfully deeper */
            } else {
                i++;
            }
        }

        node_add_child(root, newn);
        return -1;
    }

    /* cmp == 0 → equal priority, incomparable → siblings */
    return 0;
}

static void forest_insert_node(Forest *f, Node *newn)
{
    int i = 0;
    while (i < f->nroots) {
        Node *r   = f->roots[i];
        int   cmp = weighted_cmp(&r->he, &newn->he);

        if (cmp == 1) {
            /* existing root becomes child of newn */
            node_add_child(newn, r);
            forest_remove_root_at(f, i);
            /* don't advance i; slot now holds next root */
        } else if (cmp == -1) {
            int res = insert_into_node(r, newn, 1);
            if (res == 1) {
                node_add_child(newn, r);
                forest_remove_root_at(f, i);
            } else if (res == -1) {
                return; /* done */
            } else {
                i++;
            }
        } else {
            i++;
        }
    }

    /* No existing tree accepted newn → new root */
    forest_add_root(f, newn);
}

/* ========== VERTEX NORMALIZATION ========== */

static int cmp_int(const void *a, const void *b)
{
    return *(int*)a - *(int*)b;
}

static int *normalize_vertices(const int *in, int n_in, int *n_out)
{
    if (n_in == 0) { *n_out = 0; return NULL; }
    int *a = malloc(sizeof(int) * n_in);
    if (!a) { perror("malloc"); exit(1); }
    memcpy(a, in, sizeof(int) * n_in);
    qsort(a, n_in, sizeof(int), cmp_int);
    /* deduplicate */
    int w = 1;
    for (int i = 1; i < n_in; ++i)
        if (a[i] != a[w-1]) a[w++] = a[i];
    a      = realloc(a, sizeof(int) * w);
    *n_out = w;
    return a;
}

/* ========== PUBLIC API ========== */

Forest *forest_create(void)
{
    Forest *f = malloc(sizeof(Forest));
    if (!f) { perror("malloc"); exit(1); }
    f->roots     = NULL;
    f->nroots    = 0;
    f->roots_cap = 0;
    f->root_heap = heap_create();
    return f;
}

void forest_free(Forest *f)
{
    if (!f) return;
    for (int i = 0; i < f->nroots; ++i) node_free(f->roots[i]);
    free(f->roots);
    heap_free(f->root_heap);
    free(f);
}

void insert_hyperedge(Forest *f, const int *verts, int nverts, double weight)
{
    int  n_norm;
    int *norm = normalize_vertices(verts, nverts, &n_norm);
    if (n_norm == 0) { free(norm); return; }
    Node *nd = node_create(norm, n_norm, weight);
    free(norm);
    forest_insert_node(f, nd);
}

/*
 * find_top_k — O(k log k) via working max-heap.
 *
 * Seed the heap with all roots, then repeatedly pop the heaviest node
 * and push its children.  The heap never exceeds k * max_branching entries
 * in practice, and we stop after k pops.
 */
Node **find_top_k(Forest *f, int k, int *result_count)
{
    if (k <= 0 || f->nroots == 0) { *result_count = 0; return NULL; }

    Node    **result = malloc(sizeof(Node*) * k);
    if (!result) { perror("malloc"); exit(1); }

    NodeHeap *wh = heap_create();
    for (int i = 0; i < f->nroots; ++i)
        heap_push(wh, f->roots[i]);

    int count = 0;
    while (count < k && wh->size > 0) {
        Node *top     = heap_pop(wh);
        result[count++] = top;
        for (int i = 0; i < top->nchildren; ++i)
            heap_push(wh, top->children[i]);
    }

    heap_free(wh);
    *result_count = count;
    return result;
}

static int count_by_threshold_recursive(Node *nd, double threshold)
{
    if (nd->he.weight < threshold) return 0;
    int count = 1;
    for (int i = 0; i < nd->nchildren; ++i)
        count += count_by_threshold_recursive(nd->children[i], threshold);
    return count;
}

int find_by_weight_threshold(Forest *f, double threshold)
{
    int count = 0;
    for (int i = 0; i < f->nroots; ++i)
        count += count_by_threshold_recursive(f->roots[i], threshold);
    return count;
}

static Node *find_minimal_superset_recursive(Node *nd, const int *query,
                                             int nquery, Node *best)
{
    if (is_subset(query, nquery, nd->he.verts, nd->he.nverts)) {
        if (!best || nd->he.nverts < best->he.nverts) best = nd;
        for (int i = 0; i < nd->nchildren; ++i) {
            Node *cb = find_minimal_superset_recursive(nd->children[i],
                                                       query, nquery, best);
            if (cb && (!best || cb->he.nverts < best->he.nverts)) best = cb;
        }
    }
    return best;
}

Node *find_minimal_superset(Forest *f, const int *query, int nquery)
{
    Node *best = NULL;
    for (int i = 0; i < f->nroots; ++i) {
        Node *c = find_minimal_superset_recursive(f->roots[i], query,
                                                  nquery, best);
        if (c && (!best || c->he.nverts < best->he.nverts)) best = c;
    }
    return best;
}

static Node *find_heaviest_superset_recursive(Node *nd, const int *query,
                                              int nquery, Node *best)
{
    if (is_subset(query, nquery, nd->he.verts, nd->he.nverts)) {
        if (!best || nd->he.weight > best->he.weight) best = nd;
        for (int i = 0; i < nd->nchildren; ++i) {
            Node *cb = find_heaviest_superset_recursive(nd->children[i],
                                                        query, nquery, best);
            if (cb && (!best || cb->he.weight > best->he.weight)) best = cb;
        }
    }
    return best;
}

Node *find_heaviest_superset(Forest *f, const int *query, int nquery)
{
    Node *best = NULL;
    for (int i = 0; i < f->nroots; ++i) {
        Node *c = find_heaviest_superset_recursive(f->roots[i], query,
                                                   nquery, best);
        if (c && (!best || c->he.weight > best->he.weight)) best = c;
    }
    return best;
}

/* ========== CLUSTERING & ANALYSIS ========== */

static void collect_by_threshold_recursive(Node *nd, double threshold,
                                           Node ***result, int *count, int *cap)
{
    if (nd->he.weight >= threshold) {
        if (*count >= *cap) {
            *cap    = *cap ? *cap * 2 : 16;
            *result = realloc(*result, sizeof(Node*) * (*cap));
            if (!*result) { perror("realloc"); exit(1); }
        }
        (*result)[(*count)++] = nd;
        for (int i = 0; i < nd->nchildren; ++i)
            collect_by_threshold_recursive(nd->children[i], threshold,
                                           result, count, cap);
    }
}

Node **get_clusters_by_weight(Forest *f, double threshold, int *cluster_count)
{
    Node **result = NULL;
    int count = 0, cap = 0;
    for (int i = 0; i < f->nroots; ++i)
        collect_by_threshold_recursive(f->roots[i], threshold,
                                       &result, &count, &cap);
    *cluster_count = count;
    return result;
}

double compute_overlap(const Node *a, const Node *b)
{
    return overlap_ratio(a->he.verts, a->he.nverts,
                         b->he.verts, b->he.nverts);
}

/* ========== UTILITY ========== */

static int count_nodes_recursive(Node *nd)
{
    int c = 1;
    for (int i = 0; i < nd->nchildren; ++i)
        c += count_nodes_recursive(nd->children[i]);
    return c;
}

int count_total_nodes(Forest *f)
{
    int total = 0;
    for (int i = 0; i < f->nroots; ++i)
        total += count_nodes_recursive(f->roots[i]);
    return total;
}

int forest_max_depth(Forest *f)
{
    int mx = 0;
    for (int i = 0; i < f->nroots; ++i) {
        int d = node_depth(f->roots[i]);
        if (d > mx) mx = d;
    }
    return mx;
}

/* O(1): heaviest root is always at top of root_heap. */
double forest_max_weight(Forest *f)
{
    if (f->nroots == 0) return 0.0;
    return f->root_heap->data[0]->he.weight;
}

static void find_min_weight_recursive(Node *nd, double *mn)
{
    if (nd->he.weight < *mn) *mn = nd->he.weight;
    for (int i = 0; i < nd->nchildren; ++i)
        find_min_weight_recursive(nd->children[i], mn);
}

double forest_min_weight(Forest *f)
{
    if (f->nroots == 0) return 0.0;
    double mn = f->roots[0]->he.weight;
    for (int i = 0; i < f->nroots; ++i)
        find_min_weight_recursive(f->roots[i], &mn);
    return mn;
}

static void print_indent(int depth)
{
    for (int i = 0; i < depth; ++i) printf("  ");
}

static void print_node(Node *nd, int depth)
{
    print_indent(depth);
    printf("⚡ w=%.2f {", nd->he.weight);
    for (int i = 0; i < nd->he.nverts; ++i) {
        printf("%d", nd->he.verts[i]);
        if (i + 1 < nd->he.nverts) printf(",");
    }
    printf("}\n");
    for (int i = 0; i < nd->nchildren; ++i)
        print_node(nd->children[i], depth + 1);
}

void print_forest(Forest *f)
{
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║  WEIGHTED HYPERGRAPH DECOMPOSITION      ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("Roots: %d | Total: %d | Depth: %d\n",
           f->nroots, count_total_nodes(f), forest_max_depth(f));
    if (f->nroots > 0)
        printf("Weight range: [%.2f, %.2f]\n",
               forest_min_weight(f), forest_max_weight(f));

    for (int i = 0; i < f->nroots; ++i) {
        printf("\n[ROOT %d]\n", i + 1);
        print_node(f->roots[i], 0);
    }
    printf("\n");
}

static int verify_weight_monotonicity(Node *nd)
{
    for (int i = 0; i < nd->nchildren; ++i) {
        if (nd->children[i]->he.weight > nd->he.weight + 1e-9) return 0;
        if (!verify_weight_monotonicity(nd->children[i]))       return 0;
    }
    return 1;
}

int verify_forest(Forest *f)
{
    for (int i = 0; i < f->nroots; ++i)
        if (!verify_weight_monotonicity(f->roots[i])) return 0;
    return 1;
}

ForestStats get_forest_stats(Forest *f)
{
    ForestStats stats = {0};
    stats.total_nodes = count_total_nodes(f);
    stats.num_roots   = f->nroots;
    stats.max_depth   = forest_max_depth(f);
    stats.max_weight  = forest_max_weight(f);
    stats.min_weight  = forest_min_weight(f);

    if (stats.total_nodes == 0) return stats;

    /* BFS to collect avg_weight and max_children */
    Node **queue = malloc(sizeof(Node*) * (stats.total_nodes + 1));
    if (!queue) { perror("malloc"); exit(1); }

    int    front = 0, back = 0;
    double sum   = 0.0;

    for (int i = 0; i < f->nroots; ++i) queue[back++] = f->roots[i];

    while (front < back) {
        Node *curr = queue[front++];
        sum += curr->he.weight;
        if (curr->nchildren > stats.max_children)
            stats.max_children = curr->nchildren;
        for (int i = 0; i < curr->nchildren; ++i) {
            if (back <= stats.total_nodes)   /* fixed off-by-one */
                queue[back++] = curr->children[i];
        }
    }

    free(queue);
    stats.avg_weight = sum / stats.total_nodes;
    return stats;
}

/* ========== ADVANCED QUERY OPERATIONS ========== */

static void collect_supersets_recursive(Node *nd, const int *query, int nquery,
                                        Node ***result, int *count, int *cap)
{
    if (is_subset(query, nquery, nd->he.verts, nd->he.nverts)) {
        if (*count >= *cap) {
            *cap    = *cap ? *cap * 2 : 16;
            *result = realloc(*result, sizeof(Node*) * (*cap));
            if (!*result) { perror("realloc"); exit(1); }
        }
        (*result)[(*count)++] = nd;
        for (int i = 0; i < nd->nchildren; ++i)
            collect_supersets_recursive(nd->children[i], query, nquery,
                                        result, count, cap);
    }
}

Node **find_all_supersets(Forest *f, const int *query, int nquery,
                          int *result_count)
{
    Node **result = NULL;
    int count = 0, cap = 0;
    for (int i = 0; i < f->nroots; ++i)
        collect_supersets_recursive(f->roots[i], query, nquery,
                                    &result, &count, &cap);
    *result_count = count;
    return result;
}

static void collect_subsets_recursive(Node *nd, const int *query, int nquery,
                                      Node ***result, int *count, int *cap)
{
    if (is_subset(nd->he.verts, nd->he.nverts, query, nquery)) {
        if (*count >= *cap) {
            *cap    = *cap ? *cap * 2 : 16;
            *result = realloc(*result, sizeof(Node*) * (*cap));
            if (!*result) { perror("realloc"); exit(1); }
        }
        (*result)[(*count)++] = nd;
    }
    for (int i = 0; i < nd->nchildren; ++i)
        collect_subsets_recursive(nd->children[i], query, nquery,
                                  result, count, cap);
}

Node **find_all_subsets(Forest *f, const int *query, int nquery,
                        int *result_count)
{
    Node **result = NULL;
    int count = 0, cap = 0;
    for (int i = 0; i < f->nroots; ++i)
        collect_subsets_recursive(f->roots[i], query, nquery,
                                  &result, &count, &cap);
    *result_count = count;
    return result;
}

static void collect_by_weight_range_recursive(Node *nd,
                                              double min_w, double max_w,
                                              Node ***result,
                                              int *count, int *cap)
{
    if (nd->he.weight >= min_w && nd->he.weight <= max_w) {
        if (*count >= *cap) {
            *cap    = *cap ? *cap * 2 : 16;
            *result = realloc(*result, sizeof(Node*) * (*cap));
            if (!*result) { perror("realloc"); exit(1); }
        }
        (*result)[(*count)++] = nd;
    }
    if (nd->he.weight >= min_w) {
        for (int i = 0; i < nd->nchildren; ++i)
            collect_by_weight_range_recursive(nd->children[i], min_w, max_w,
                                              result, count, cap);
    }
}

Node **find_by_weight_range(Forest *f, double min_weight, double max_weight,
                            int *result_count)
{
    Node **result = NULL;
    int count = 0, cap = 0;
    for (int i = 0; i < f->nroots; ++i)
        if (f->roots[i]->he.weight >= min_weight)
            collect_by_weight_range_recursive(f->roots[i], min_weight,
                                              max_weight, &result,
                                              &count, &cap);
    *result_count = count;
    return result;
}

static void collect_containing_recursive(Node *nd,
                                         const int *vertices, int nvertices,
                                         Node ***result, int *count, int *cap)
{
    if (is_subset(vertices, nvertices, nd->he.verts, nd->he.nverts)) {
        if (*count >= *cap) {
            *cap    = *cap ? *cap * 2 : 16;
            *result = realloc(*result, sizeof(Node*) * (*cap));
            if (!*result) { perror("realloc"); exit(1); }
        }
        (*result)[(*count)++] = nd;
        for (int i = 0; i < nd->nchildren; ++i)
            collect_containing_recursive(nd->children[i], vertices, nvertices,
                                         result, count, cap);
    }
}

Node **find_containing_vertices(Forest *f, const int *vertices, int nvertices,
                                int *result_count)
{
    Node **result = NULL;
    int count = 0, cap = 0;
    for (int i = 0; i < f->nroots; ++i)
        collect_containing_recursive(f->roots[i], vertices, nvertices,
                                     &result, &count, &cap);
    *result_count = count;
    return result;
}

/* ---- similarity search ---- */

typedef struct { Node *node; double similarity; } SimilarityPair;

static int cmp_similarity(const void *a, const void *b)
{
    double da = ((SimilarityPair*)a)->similarity;
    double db = ((SimilarityPair*)b)->similarity;
    return (da < db) - (da > db); /* descending */
}

static void collect_all_nodes_recursive(Node *nd,
                                        Node ***result, int *count, int *cap)
{
    if (*count >= *cap) {
        *cap    = *cap ? *cap * 2 : 64;
        *result = realloc(*result, sizeof(Node*) * (*cap));
        if (!*result) { perror("realloc"); exit(1); }
    }
    (*result)[(*count)++] = nd;
    for (int i = 0; i < nd->nchildren; ++i)
        collect_all_nodes_recursive(nd->children[i], result, count, cap);
}

static Node **collect_all_nodes(Forest *f, int *total_count)
{
    Node **all  = NULL;
    int count   = 0, cap = 0;
    for (int i = 0; i < f->nroots; ++i)
        collect_all_nodes_recursive(f->roots[i], &all, &count, &cap);
    *total_count = count;
    return all;
}

Node **find_k_most_similar(Forest *f, const int *query, int nquery,
                           int k, int *result_count)
{
    int total;
    Node **all = collect_all_nodes(f, &total);
    if (total == 0) { *result_count = 0; return NULL; }

    SimilarityPair *pairs = malloc(sizeof(SimilarityPair) * total);
    if (!pairs) { perror("malloc"); exit(1); }

    for (int i = 0; i < total; ++i) {
        pairs[i].node       = all[i];
        pairs[i].similarity = overlap_ratio(query, nquery,
                                            all[i]->he.verts,
                                            all[i]->he.nverts);
    }
    qsort(pairs, total, sizeof(SimilarityPair), cmp_similarity);

    int sz      = k < total ? k : total;
    Node **result = malloc(sizeof(Node*) * sz);
    if (!result) { perror("malloc"); exit(1); }
    for (int i = 0; i < sz; ++i) result[i] = pairs[i].node;

    free(pairs);
    free(all);
    *result_count = sz;
    return result;
}

/* ========== OPTIMIZATION & MAINTENANCE ========== */

static int cmp_by_weight_desc(const void *a, const void *b)
{
    double wa = (*(Node**)a)->he.weight;
    double wb = (*(Node**)b)->he.weight;
    return (wa < wb) - (wa > wb); /* descending */
}

void forest_rebalance(Forest *f)
{
    int total;
    Node **all = collect_all_nodes(f, &total);
    if (total == 0) { free(all); return; }

    qsort(all, total, sizeof(Node*), cmp_by_weight_desc);

    /* Detach every node from its children before reinserting */
    for (int i = 0; i < total; ++i) {
        free(all[i]->children);        /* free the pointer array itself */
        all[i]->children     = NULL;
        all[i]->nchildren    = 0;
        all[i]->children_cap = 0;
    }

    /* Reset forest root list and heap */
    f->nroots          = 0;
    f->root_heap->size = 0;

    for (int i = 0; i < total; ++i)
        forest_insert_node(f, all[i]);

    free(all);
}

static int vertices_equal(const int *a, int na, const int *b, int nb)
{
    if (na != nb) return 0;
    for (int i = 0; i < na; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

int forest_merge_duplicates(Forest *f, int keep_max)
{
    int total;
    Node **all = collect_all_nodes(f, &total);
    if (total <= 1) { free(all); return 0; }

    int *processed    = calloc(total, sizeof(int));
    if (!processed) { perror("calloc"); exit(1); }
    int  merged_count = 0;

    for (int i = 0; i < total; ++i) {
        if (processed[i]) continue;
        processed[i] = 1;

        double weight_sum = all[i]->he.weight;
        double weight_max = all[i]->he.weight;
        int    dup_count  = 1;

        for (int j = i + 1; j < total; ++j) {
            if (processed[j]) continue;
            if (!vertices_equal(all[i]->he.verts, all[i]->he.nverts,
                                all[j]->he.verts, all[j]->he.nverts))
                continue;

            weight_sum += all[j]->he.weight;
            if (all[j]->he.weight > weight_max)
                weight_max = all[j]->he.weight;

            processed[j] = 1;
            dup_count++;
            merged_count++;
        }

        /* Apply merged weight once, after scanning all duplicates */
        if (dup_count > 1)
            all[i]->he.weight = keep_max ? weight_max
                                         : weight_sum / dup_count;
    }

    free(processed);
    free(all);

    if (merged_count > 0) forest_rebalance(f);
    return merged_count;
}

static void prune_children(Node *nd, double threshold, int *removed)
{
    int i = 0;
    while (i < nd->nchildren) {
        if (nd->children[i]->he.weight < threshold) {
            node_free(nd->children[i]);
            for (int j = i; j + 1 < nd->nchildren; ++j)
                nd->children[j] = nd->children[j+1];
            nd->nchildren--;
            (*removed)++;
        } else {
            prune_children(nd->children[i], threshold, removed);
            i++;
        }
    }
}

int forest_prune_by_weight(Forest *f, double threshold)
{
    int removed = 0, i = 0;
    while (i < f->nroots) {
        if (f->roots[i]->he.weight < threshold) {
            node_free(f->roots[i]);
            forest_remove_root_at(f, i);
            removed++;
        } else {
            prune_children(f->roots[i], threshold, &removed);
            i++;
        }
    }
    return removed;
}

void forest_optimize(Forest *f)
{
    forest_merge_duplicates(f, 1);
    forest_rebalance(f);
}

/* ========== BATCH OPERATIONS ========== */

/*
 * forest_insert_batch — sorts a copy of the edge array by weight descending
 * before inserting, which produces a shallower tree than random order.
 */
void forest_insert_batch(Forest *f, Hyperedge *edges, int nedges)
{
    if (nedges <= 0) return;

    Hyperedge *sorted = malloc(sizeof(Hyperedge) * nedges);
    if (!sorted) { perror("malloc"); exit(1); }
    memcpy(sorted, edges, sizeof(Hyperedge) * nedges);

    /* sort by weight descending using the same comparator style */
    qsort(sorted, nedges, sizeof(Hyperedge),
          (int(*)(const void*, const void*))cmp_by_weight_desc);

    for (int i = 0; i < nedges; ++i)
        insert_hyperedge(f, sorted[i].verts, sorted[i].nverts,
                         sorted[i].weight);

    free(sorted);
}

Forest *forest_build_bulk(Hyperedge *edges, int nedges)
{
    Forest *f = forest_create();
    forest_insert_batch(f, edges, nedges);
    return f;
}

/* ========== SERIALIZATION ========== */

static void write_node(Node *nd, FILE *fp)
{
    fwrite(&nd->he.nverts,  sizeof(int),    1,             fp);
    fwrite(nd->he.verts,    sizeof(int),    nd->he.nverts, fp);
    fwrite(&nd->he.weight,  sizeof(double), 1,             fp);
    fwrite(&nd->nchildren,  sizeof(int),    1,             fp);
    for (int i = 0; i < nd->nchildren; ++i) write_node(nd->children[i], fp);
}

int forest_save(Forest *f, const char *filename)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    fwrite(&f->nroots, sizeof(int), 1, fp);
    for (int i = 0; i < f->nroots; ++i) write_node(f->roots[i], fp);
    fclose(fp);
    return 0;
}

static Node *read_node(FILE *fp)
{
    int nverts;
    if (fread(&nverts, sizeof(int), 1, fp) != 1) return NULL;

    int *verts = malloc(sizeof(int) * nverts);
    if (!verts) return NULL;
    if ((int)fread(verts, sizeof(int), nverts, fp) != nverts) {
        free(verts); return NULL;
    }

    double weight;
    if (fread(&weight, sizeof(double), 1, fp) != 1) {
        free(verts); return NULL;
    }

    Node *nd = node_create(verts, nverts, weight);
    free(verts);

    int nchildren;
    if (fread(&nchildren, sizeof(int), 1, fp) != 1) {
        node_free(nd); return NULL;
    }

    for (int i = 0; i < nchildren; ++i) {
        Node *child = read_node(fp);
        if (!child) { node_free(nd); return NULL; }
        node_add_child(nd, child);
    }
    return nd;
}

Forest *forest_load(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    Forest *f = forest_create();
    int nroots;
    if (fread(&nroots, sizeof(int), 1, fp) != 1) {
        forest_free(f); fclose(fp); return NULL;
    }

    for (int i = 0; i < nroots; ++i) {
        Node *root = read_node(fp);
        if (!root) { forest_free(f); fclose(fp); return NULL; }
        forest_add_root(f, root);
    }

    fclose(fp);
    return f;
}

/* ========== TRAVERSAL ========== */

void forest_traverse_bfs(Forest *f, NodeVisitor visitor, void *user_data)
{
    if (!f || !visitor || f->nroots == 0) return;

    int    total = count_total_nodes(f);
    Node **queue = malloc(sizeof(Node*) * (total + 1));
    if (!queue) return;

    int front = 0, back = 0;
    for (int i = 0; i < f->nroots; ++i) queue[back++] = f->roots[i];

    while (front < back) {
        Node *curr = queue[front++];
        if (visitor(curr, user_data) != 0) break;
        for (int i = 0; i < curr->nchildren; ++i)
            if (back <= total) queue[back++] = curr->children[i];
    }

    free(queue);
}

static int dfs_helper(Node *nd, NodeVisitor visitor, void *user_data)
{
    if (visitor(nd, user_data) != 0) return 1;
    for (int i = 0; i < nd->nchildren; ++i)
        if (dfs_helper(nd->children[i], visitor, user_data)) return 1;
    return 0;
}

void forest_traverse_dfs(Forest *f, NodeVisitor visitor, void *user_data)
{
    if (!f || !visitor) return;
    for (int i = 0; i < f->nroots; ++i)
        if (dfs_helper(f->roots[i], visitor, user_data)) break;
}

void forest_traverse_by_weight(Forest *f, NodeVisitor visitor, void *user_data)
{
    if (!f || !visitor) return;
    int    total;
    Node **all = collect_all_nodes(f, &total);
    if (total == 0) { free(all); return; }

    qsort(all, total, sizeof(Node*), cmp_by_weight_desc);
    for (int i = 0; i < total; ++i)
        if (visitor(all[i], user_data) != 0) break;

    free(all);
}
