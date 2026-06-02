/**
 * Hyperedge Inclusion Forest (HIF)
 * Weight-Based Hierarchical Decomposition
 *
 * Changes from initial version:
 *   - Added NodeHeap (max-heap by weight) embedded in Forest
 *   - find_top_k: uses a lazy global weight index
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== TUNING PARAMETERS ========== */

#define WEIGHT_EPS 1e-9

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

/*
 * A tree edge is valid only when the parent is both at least as heavy
 * and a superset of the child. This keeps query pruning correct.
 */
static int can_parent(const Hyperedge *parent, const Hyperedge *child)
{
    if (!parent || !child) return 0;
    if (parent->weight + WEIGHT_EPS < child->weight) return 0;
    return is_subset(child->verts, child->nverts, parent->verts, parent->nverts);
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

/* ========== INDEX IMPLEMENTATION ========== */

static unsigned hash_int(int value)
{
    unsigned x = (unsigned)value;
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static void forest_init_indexes(Forest *f)
{
    f->nodes = NULL;
    f->nnodes = 0;
    f->nodes_cap = 0;
    f->weight_order = NULL;
    f->weight_order_dirty = 1;
    f->vertex_nbuckets = 1024;
    f->vertex_buckets = calloc((size_t)f->vertex_nbuckets,
                               sizeof(VertexIndexEntry*));
    if (!f->vertex_buckets) { perror("calloc"); exit(1); }
}

static void vertex_entry_free(VertexIndexEntry *entry)
{
    while (entry) {
        VertexIndexEntry *next = entry->next;
        free(entry->nodes);
        free(entry);
        entry = next;
    }
}

static void forest_clear_indexes(Forest *f)
{
    if (!f) return;
    free(f->nodes);
    f->nodes = NULL;
    f->nnodes = 0;
    f->nodes_cap = 0;

    free(f->weight_order);
    f->weight_order = NULL;
    f->weight_order_dirty = 1;

    if (f->vertex_buckets) {
        for (int i = 0; i < f->vertex_nbuckets; ++i)
            vertex_entry_free(f->vertex_buckets[i]);
        free(f->vertex_buckets);
    }
    f->vertex_buckets = NULL;
    f->vertex_nbuckets = 0;
}

static VertexIndexEntry *forest_vertex_entry(Forest *f, int vertex, int create)
{
    unsigned bucket = hash_int(vertex) % (unsigned)f->vertex_nbuckets;
    VertexIndexEntry *entry = f->vertex_buckets[bucket];
    while (entry) {
        if (entry->vertex == vertex) return entry;
        entry = entry->next;
    }
    if (!create) return NULL;

    entry = calloc(1, sizeof(VertexIndexEntry));
    if (!entry) { perror("calloc"); exit(1); }
    entry->vertex = vertex;
    entry->next = f->vertex_buckets[bucket];
    f->vertex_buckets[bucket] = entry;
    return entry;
}

static void vertex_entry_add_node(VertexIndexEntry *entry, Node *node)
{
    if (entry->nnodes >= entry->nodes_cap) {
        int newcap = entry->nodes_cap ? entry->nodes_cap * 2 : 8;
        Node **tmp = realloc(entry->nodes, sizeof(Node*) * (size_t)newcap);
        if (!tmp) { perror("realloc"); exit(1); }
        entry->nodes = tmp;
        entry->nodes_cap = newcap;
    }
    entry->nodes[entry->nnodes++] = node;
}

static void forest_index_node(Forest *f, Node *node)
{
    if (f->nnodes >= f->nodes_cap) {
        int newcap = f->nodes_cap ? f->nodes_cap * 2 : 64;
        Node **tmp = realloc(f->nodes, sizeof(Node*) * (size_t)newcap);
        if (!tmp) { perror("realloc"); exit(1); }
        f->nodes = tmp;
        f->nodes_cap = newcap;
    }
    f->nodes[f->nnodes++] = node;
    f->weight_order_dirty = 1;

    for (int i = 0; i < node->he.nverts; ++i) {
        VertexIndexEntry *entry = forest_vertex_entry(f, node->he.verts[i], 1);
        vertex_entry_add_node(entry, node);
    }
}

static void forest_index_subtree(Forest *f, Node *node)
{
    if (!node) return;
    forest_index_node(f, node);
    for (int i = 0; i < node->nchildren; ++i)
        forest_index_subtree(f, node->children[i]);
}

static void forest_rebuild_indexes(Forest *f)
{
    if (!f) return;
    forest_clear_indexes(f);
    forest_init_indexes(f);
    for (int i = 0; i < f->nroots; ++i)
        forest_index_subtree(f, f->roots[i]);
}

static int cmp_node_ptr_by_weight_desc(const void *a, const void *b)
{
    double wa = (*(Node* const*)a)->he.weight;
    double wb = (*(Node* const*)b)->he.weight;
    return (wa < wb) - (wa > wb);
}

static void forest_ensure_weight_order(Forest *f)
{
    if (!f || !f->weight_order_dirty) return;
    free(f->weight_order);
    f->weight_order = NULL;
    if (f->nnodes > 0) {
        f->weight_order = malloc(sizeof(Node*) * (size_t)f->nnodes);
        if (!f->weight_order) { perror("malloc"); exit(1); }
        memcpy(f->weight_order, f->nodes, sizeof(Node*) * (size_t)f->nnodes);
        qsort(f->weight_order, (size_t)f->nnodes, sizeof(Node*),
              cmp_node_ptr_by_weight_desc);
    }
    f->weight_order_dirty = 0;
}

static int append_node(Node ***result, int *count, int *cap, Node *node)
{
    if (*count >= *cap) {
        int newcap = *cap ? *cap * 2 : 16;
        Node **tmp = realloc(*result, sizeof(Node*) * (size_t)newcap);
        if (!tmp) return 0;
        *result = tmp;
        *cap = newcap;
    }
    (*result)[(*count)++] = node;
    return 1;
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
static int insert_into_node(Node *root, Node *newn)
{
    if (can_parent(&newn->he, &root->he))
        return 1;

    if (!can_parent(&root->he, &newn->he))
        return 0;

    int i = 0;
    while (i < root->nchildren) {
        int res = insert_into_node(root->children[i], newn);
        if (res == 1) {
            Node *child = root->children[i];
            for (int j = i; j + 1 < root->nchildren; ++j)
                root->children[j] = root->children[j+1];
            root->nchildren--;
            node_add_child(newn, child);
        } else if (res == -1) {
            return -1;
        } else {
            i++;
        }
    }

    node_add_child(root, newn);
    return -1;
}

static void forest_place_node(Forest *f, Node *newn)
{
    int i = 0;
    while (i < f->nroots) {
        Node *r = f->roots[i];

        if (can_parent(&newn->he, &r->he)) {
            node_add_child(newn, r);
            forest_remove_root_at(f, i);
        } else if (can_parent(&r->he, &newn->he)) {
            int res = insert_into_node(r, newn);
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

static void forest_insert_node(Forest *f, Node *newn)
{
    forest_place_node(f, newn);
    forest_index_node(f, newn);
}

/* ========== VERTEX NORMALIZATION ========== */

static int cmp_int(const void *a, const void *b)
{
    int A = *(const int*)a;
    int B = *(const int*)b;
    return (A > B) - (A < B);
}

static int *normalize_vertices(const int *in, int n_in, int *n_out)
{
    if (!n_out) return NULL;
    if (!in || n_in <= 0) { *n_out = 0; return NULL; }
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

static int normalize_query(const int *query, int nquery, int **norm, int *nnorm)
{
    if (!norm || !nnorm) return 0;
    *norm = NULL;
    *nnorm = 0;
    if (nquery < 0 || (!query && nquery > 0)) return 0;
    if (nquery == 0) return 1;
    *norm = normalize_vertices(query, nquery, nnorm);
    return 1;
}

static Node **find_superset_candidates(Forest *f, const int *query, int nquery,
                                       int *result_count)
{
    Node **result = NULL;
    int count = 0, cap = 0;

    if (nquery == 0) {
        if (f->nnodes > 0) {
            result = malloc(sizeof(Node*) * (size_t)f->nnodes);
            if (!result) { perror("malloc"); exit(1); }
            memcpy(result, f->nodes, sizeof(Node*) * (size_t)f->nnodes);
            count = f->nnodes;
        }
        *result_count = count;
        return result;
    }

    VertexIndexEntry *shortest = NULL;
    for (int i = 0; i < nquery; ++i) {
        VertexIndexEntry *entry = forest_vertex_entry(f, query[i], 0);
        if (!entry) {
            *result_count = 0;
            return NULL;
        }
        if (!shortest || entry->nnodes < shortest->nnodes)
            shortest = entry;
    }

    for (int i = 0; i < shortest->nnodes; ++i) {
        Node *candidate = shortest->nodes[i];
        if (!is_subset(query, nquery, candidate->he.verts,
                       candidate->he.nverts))
            continue;
        if (!append_node(&result, &count, &cap, candidate)) {
            perror("realloc"); exit(1);
        }
    }

    *result_count = count;
    return result;
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
    forest_init_indexes(f);
    return f;
}

void forest_free(Forest *f)
{
    if (!f) return;
    for (int i = 0; i < f->nroots; ++i) node_free(f->roots[i]);
    free(f->roots);
    heap_free(f->root_heap);
    forest_clear_indexes(f);
    free(f);
}

void insert_hyperedge(Forest *f, const int *verts, int nverts, double weight)
{
    if (!f) return;
    int  n_norm;
    int *norm = normalize_vertices(verts, nverts, &n_norm);
    if (n_norm == 0) { free(norm); return; }
    Node *nd = node_create(norm, n_norm, weight);

    int candidate_count;
    Node **candidates = find_superset_candidates(f, norm, n_norm,
                                                 &candidate_count);
    Node *parent = NULL;
    for (int i = 0; i < candidate_count; ++i) {
        Node *candidate = candidates[i];
        if (!can_parent(&candidate->he, &nd->he)) continue;
        if (!parent ||
            candidate->he.nverts < parent->he.nverts ||
            (candidate->he.nverts == parent->he.nverts &&
             candidate->he.weight < parent->he.weight)) {
            parent = candidate;
        }
    }

    if (parent && insert_into_node(parent, nd) == -1) {
        forest_index_node(f, nd);
        free(candidates);
        free(norm);
        return;
    }

    free(candidates);
    free(norm);
    forest_insert_node(f, nd);
}

/* find_top_k: O(n log n) when rebuilding the lazy weight index, O(k) after. */
Node **find_top_k(Forest *f, int k, int *result_count)
{
    if (result_count) *result_count = 0;
    if (!f || k <= 0 || f->nnodes == 0) return NULL;

    forest_ensure_weight_order(f);

    int count = k < f->nnodes ? k : f->nnodes;
    Node **result = malloc(sizeof(Node*) * (size_t)count);
    if (!result) { perror("malloc"); exit(1); }
    memcpy(result, f->weight_order, sizeof(Node*) * (size_t)count);
    if (result_count) *result_count = count;
    return result;
}

int find_by_weight_threshold(Forest *f, double threshold)
{
    if (!f) return 0;
    forest_ensure_weight_order(f);
    int count = 0;
    while (count < f->nnodes && f->weight_order[count]->he.weight >= threshold)
        count++;
    return count;
}

Node *find_minimal_superset(Forest *f, const int *query, int nquery)
{
    if (!f) return NULL;
    int n_norm;
    int *norm;
    if (!normalize_query(query, nquery, &norm, &n_norm)) return NULL;

    int count;
    Node **candidates = find_superset_candidates(f, norm, n_norm, &count);
    Node *best = NULL;
    for (int i = 0; i < count; ++i) {
        Node *c = candidates[i];
        if (!best || c->he.nverts < best->he.nverts ||
            (c->he.nverts == best->he.nverts &&
             c->he.weight > best->he.weight)) {
            best = c;
        }
    }
    free(candidates);
    free(norm);
    return best;
}

Node *find_heaviest_superset(Forest *f, const int *query, int nquery)
{
    if (!f) return NULL;
    int n_norm;
    int *norm;
    if (!normalize_query(query, nquery, &norm, &n_norm)) return NULL;

    int count;
    Node **candidates = find_superset_candidates(f, norm, n_norm, &count);
    Node *best = NULL;
    for (int i = 0; i < count; ++i)
        if (!best || candidates[i]->he.weight > best->he.weight)
            best = candidates[i];
    free(candidates);
    free(norm);
    return best;
}

/* ========== CLUSTERING & ANALYSIS ========== */

Node **get_clusters_by_weight(Forest *f, double threshold, int *cluster_count)
{
    if (cluster_count) *cluster_count = 0;
    if (!f) return NULL;
    Node **result = NULL;
    int count = 0, cap = 0;
    forest_ensure_weight_order(f);
    for (int i = 0; i < f->nnodes; ++i) {
        if (f->weight_order[i]->he.weight < threshold) break;
        if (!append_node(&result, &count, &cap, f->weight_order[i])) {
            perror("realloc"); exit(1);
        }
    }
    if (cluster_count) *cluster_count = count;
    return result;
}

double compute_overlap(const Node *a, const Node *b)
{
    if (!a || !b) return 0.0;
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
    if (!f) return 0;
    return f->nnodes;
}

int forest_max_depth(Forest *f)
{
    if (!f) return 0;
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
    if (!f || f->nnodes == 0) return 0.0;
    forest_ensure_weight_order(f);
    return f->weight_order[0]->he.weight;
}

double forest_min_weight(Forest *f)
{
    if (!f || f->nnodes == 0) return 0.0;
    forest_ensure_weight_order(f);
    return f->weight_order[f->nnodes - 1]->he.weight;
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
    if (!f) return;
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
        if (!is_subset(nd->children[i]->he.verts, nd->children[i]->he.nverts,
                       nd->he.verts, nd->he.nverts)) return 0;
        if (!verify_weight_monotonicity(nd->children[i]))       return 0;
    }
    return 1;
}

int verify_forest(Forest *f)
{
    if (!f) return 1;
    for (int i = 0; i < f->nroots; ++i)
        if (!verify_weight_monotonicity(f->roots[i])) return 0;
    return 1;
}

ForestStats get_forest_stats(Forest *f)
{
    ForestStats stats = {0};
    if (!f) return stats;
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

Node **find_all_supersets(Forest *f, const int *query, int nquery,
                          int *result_count)
{
    if (result_count) *result_count = 0;
    if (!f) return NULL;
    int n_norm;
    int *norm;
    if (!normalize_query(query, nquery, &norm, &n_norm)) return NULL;

    int count;
    Node **result = find_superset_candidates(f, norm, n_norm, &count);
    free(norm);
    if (result_count) *result_count = count;
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
    if (result_count) *result_count = 0;
    if (!f) return NULL;
    int n_norm;
    int *norm;
    if (!normalize_query(query, nquery, &norm, &n_norm)) return NULL;

    Node **result = NULL;
    int count = 0, cap = 0;
    for (int i = 0; i < f->nroots; ++i)
        collect_subsets_recursive(f->roots[i], norm, n_norm,
                                  &result, &count, &cap);
    free(norm);
    if (result_count) *result_count = count;
    return result;
}

Node **find_by_weight_range(Forest *f, double min_weight, double max_weight,
                            int *result_count)
{
    if (result_count) *result_count = 0;
    if (!f || min_weight > max_weight) return NULL;
    Node **result = NULL;
    int count = 0, cap = 0;
    forest_ensure_weight_order(f);
    for (int i = 0; i < f->nnodes; ++i) {
        Node *node = f->weight_order[i];
        if (node->he.weight < min_weight) break;
        if (node->he.weight <= max_weight &&
            !append_node(&result, &count, &cap, node)) {
            perror("realloc"); exit(1);
        }
    }
    if (result_count) *result_count = count;
    return result;
}

Node **find_containing_vertices(Forest *f, const int *vertices, int nvertices,
                                int *result_count)
{
    if (result_count) *result_count = 0;
    if (!f) return NULL;
    int n_norm;
    int *norm;
    if (!normalize_query(vertices, nvertices, &norm, &n_norm)) return NULL;

    int count;
    Node **result = find_superset_candidates(f, norm, n_norm, &count);
    free(norm);
    if (result_count) *result_count = count;
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

static Node **collect_all_nodes(Forest *f, int *total_count)
{
    Node **all = NULL;
    int count = f ? f->nnodes : 0;
    if (count > 0) {
        all = malloc(sizeof(Node*) * (size_t)count);
        if (!all) { perror("malloc"); exit(1); }
        memcpy(all, f->nodes, sizeof(Node*) * (size_t)count);
    }
    *total_count = count;
    return all;
}

Node **find_k_most_similar(Forest *f, const int *query, int nquery,
                           int k, int *result_count)
{
    if (result_count) *result_count = 0;
    if (!f || k <= 0) return NULL;
    int n_norm;
    int *norm;
    if (!normalize_query(query, nquery, &norm, &n_norm)) return NULL;

    int total;
    Node **all = collect_all_nodes(f, &total);
    if (total == 0) { free(norm); return NULL; }

    size_t total_sz = (size_t)total;
    if (total_sz > SIZE_MAX / sizeof(SimilarityPair)) {
        free(all);
        free(norm);
        return NULL;
    }
    SimilarityPair *pairs = malloc(sizeof(SimilarityPair) * total_sz);
    if (!pairs) { perror("malloc"); exit(1); }

    for (int i = 0; i < total; ++i) {
        pairs[i].node       = all[i];
        pairs[i].similarity = overlap_ratio(norm, n_norm,
                                            all[i]->he.verts,
                                            all[i]->he.nverts);
    }
    qsort(pairs, total, sizeof(SimilarityPair), cmp_similarity);

    int sz = k < total ? k : total;
    size_t sz_sz = (size_t)sz;
    if (sz_sz > SIZE_MAX / sizeof(Node*)) {
        free(pairs);
        free(all);
        free(norm);
        return NULL;
    }
    Node **result = malloc(sizeof(Node*) * sz_sz);
    if (!result) { perror("malloc"); exit(1); }
    for (int i = 0; i < sz; ++i) result[i] = pairs[i].node;

    free(pairs);
    free(all);
    free(norm);
    if (result_count) *result_count = sz;
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
    if (!f) return;
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
        forest_place_node(f, all[i]);

    f->weight_order_dirty = 1;

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
    if (!f) return 0;
    int total;
    Node **all = collect_all_nodes(f, &total);
    if (total <= 1) { free(all); return 0; }

    Hyperedge *unique = NULL;
    int unique_count = 0, unique_cap = 0;
    int merged_count = 0;

    for (int i = 0; i < total; ++i) {
        int found = -1;
        for (int j = 0; j < unique_count; ++j) {
            if (vertices_equal(all[i]->he.verts, all[i]->he.nverts,
                               unique[j].verts, unique[j].nverts)) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            if (keep_max) {
                if (all[i]->he.weight > unique[found].weight)
                    unique[found].weight = all[i]->he.weight;
            } else {
                unique[found].weight += all[i]->he.weight;
            }
            merged_count++;
            continue;
        }

        if (unique_count >= unique_cap) {
            int newcap = unique_cap ? unique_cap * 2 : 16;
            Hyperedge *tmp = realloc(unique, sizeof(Hyperedge) * newcap);
            if (!tmp) { perror("realloc"); exit(1); }
            unique = tmp;
            unique_cap = newcap;
        }
        unique[unique_count].verts = copy_int_array(all[i]->he.verts,
                                                    all[i]->he.nverts);
        unique[unique_count].nverts = all[i]->he.nverts;
        unique[unique_count].weight = all[i]->he.weight;
        unique_count++;
    }

    if (!keep_max && merged_count > 0) {
        for (int i = 0; i < unique_count; ++i) {
            int copies = 0;
            for (int j = 0; j < total; ++j)
                if (vertices_equal(unique[i].verts, unique[i].nverts,
                                   all[j]->he.verts, all[j]->he.nverts))
                    copies++;
            if (copies > 1)
                unique[i].weight /= copies;
        }
    }

    free(all);

    if (merged_count > 0) {
        forest_clear_indexes(f);
        forest_init_indexes(f);
        for (int i = 0; i < f->nroots; ++i) node_free(f->roots[i]);
        f->nroots = 0;
        f->root_heap->size = 0;
        forest_insert_batch(f, unique, unique_count);
    }

    for (int i = 0; i < unique_count; ++i)
        free(unique[i].verts);
    free(unique);

    return merged_count;
}

static void prune_children(Node *nd, double threshold, int *removed)
{
    int i = 0;
    while (i < nd->nchildren) {
        if (nd->children[i]->he.weight < threshold) {
            int subtree_size = count_nodes_recursive(nd->children[i]);
            node_free(nd->children[i]);
            for (int j = i; j + 1 < nd->nchildren; ++j)
                nd->children[j] = nd->children[j+1];
            nd->nchildren--;
            *removed += subtree_size;
        } else {
            prune_children(nd->children[i], threshold, removed);
            i++;
        }
    }
}

int forest_prune_by_weight(Forest *f, double threshold)
{
    if (!f) return 0;
    int removed = 0, i = 0;
    while (i < f->nroots) {
        if (f->roots[i]->he.weight < threshold) {
            int subtree_size = count_nodes_recursive(f->roots[i]);
            node_free(f->roots[i]);
            forest_remove_root_at(f, i);
            removed += subtree_size;
        } else {
            prune_children(f->roots[i], threshold, &removed);
            i++;
        }
    }
    if (removed > 0)
        forest_rebuild_indexes(f);
    return removed;
}

void forest_optimize(Forest *f)
{
    if (!f) return;
    forest_merge_duplicates(f, 1);
    forest_rebalance(f);
}

static int cmp_hyperedge_by_weight_desc(const void *a, const void *b)
{
    const Hyperedge *A = (const Hyperedge*)a;
    const Hyperedge *B = (const Hyperedge*)b;
    return (A->weight < B->weight) - (A->weight > B->weight);
}

/* ========== BATCH OPERATIONS ========== */

/*
 * forest_insert_batch — sorts a copy of the edge array by weight descending
 * before inserting, which produces a shallower tree than random order.
 */
void forest_insert_batch(Forest *f, Hyperedge *edges, int nedges)
{
    if (!f || !edges || nedges <= 0) return;

    Hyperedge *sorted = malloc(sizeof(Hyperedge) * nedges);
    if (!sorted) { perror("malloc"); exit(1); }
    memcpy(sorted, edges, sizeof(Hyperedge) * nedges);

    qsort(sorted, nedges, sizeof(Hyperedge), cmp_hyperedge_by_weight_desc);

    for (int i = 0; i < nedges; ++i)
        insert_hyperedge(f, sorted[i].verts, sorted[i].nverts,
                         sorted[i].weight);

    free(sorted);
}

Forest *forest_build_bulk(Hyperedge *edges, int nedges)
{
    Forest *f = forest_create();
    if (!f) return NULL;
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
    if (!f || !filename) return -1;
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    if (fwrite(&f->nroots, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    for (int i = 0; i < f->nroots; ++i) write_node(f->roots[i], fp);
    return fclose(fp) == 0 ? 0 : -1;
}

static Node *read_node(FILE *fp)
{
    int nverts;
    if (fread(&nverts, sizeof(int), 1, fp) != 1) return NULL;
    if (nverts <= 0 || nverts > 100000000) return NULL;

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
    if (nchildren < 0 || nchildren > 100000000) {
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
    if (!filename) return NULL;
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    Forest *f = forest_create();
    int nroots;
    if (fread(&nroots, sizeof(int), 1, fp) != 1) {
        forest_free(f); fclose(fp); return NULL;
    }
    if (nroots < 0 || nroots > 100000000) {
        forest_free(f); fclose(fp); return NULL;
    }

    for (int i = 0; i < nroots; ++i) {
        Node *root = read_node(fp);
        if (!root) { forest_free(f); fclose(fp); return NULL; }
        forest_add_root(f, root);
    }

    forest_rebuild_indexes(f);

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
