#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct {
    int *verts;
    int nverts;
    double weight;
} Hyperedge;

typedef struct Node {
    Hyperedge he;
    struct Node **children;
    int nchildren;
    int children_cap;
} Node;

typedef struct {
    Node **roots;
    int nroots;
    int roots_cap;
} Forest;

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

static int subset_cmp(const Hyperedge *A, const Hyperedge *B) {
    int A_in_B = is_subset(A->verts, A->nverts, B->verts, B->nverts);
    int B_in_A = is_subset(B->verts, B->nverts, A->verts, A->nverts);
    if (A_in_B && !B_in_A) return 1;
    if (B_in_A && !A_in_B) return -1;
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

static Forest *forest_create() {
    Forest *f = malloc(sizeof(Forest));
    if (!f) { perror("malloc"); exit(1); }
    f->roots = NULL;
    f->nroots = 0;
    f->roots_cap = 0;
    return f;
}

static void forest_free(Forest *f) {
    if (!f) return;
    for (int i = 0; i < f->nroots; ++i) node_free(f->roots[i]);
    free(f->roots);
    free(f);
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

static int insert_into_node(Node *root, Node *newn) {
    int cmp = subset_cmp(&root->he, &newn->he);
    if (cmp == 1) {
        return 1;
    } else if (cmp == -1) {
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
    } else {
        return 0;
    }
}

static void forest_insert(Forest *f, Node *newn) {
    int i = 0;
    int inserted = 0;
    
    while (i < f->nroots) {
        Node *r = f->roots[i];
        int cmp = subset_cmp(&r->he, &newn->he);
        if (cmp == 1) {
            node_add_child(newn, r);
            forest_remove_root_at(f, i);
        } else if (cmp == -1) {
            int res = insert_into_node(r, newn);
            if (res == 1) {
                node_add_child(newn, r);
                forest_remove_root_at(f, i);
            } else {
                inserted = 1;
                return;
            }
        } else {
            i++;
        }
    }

    if (!inserted) {
        forest_add_root(f, newn);
    }
}

static void print_indent(int depth) {
    for (int i = 0; i < depth; ++i) putchar(' ');
}

static void print_node(Node *nd, int depth) {
    print_indent(depth);
    printf("he{w=%.2f, n=%d} : ", nd->he.weight, nd->he.nverts);
    printf("{");
    for (int i = 0; i < nd->he.nverts; ++i) {
        printf("%d", nd->he.verts[i]);
        if (i + 1 < nd->he.nverts) putchar(',');
    }
    printf("}\n");
    for (int i = 0; i < nd->nchildren; ++i) print_node(nd->children[i], depth + 2);
}

static void print_forest(Forest *f) {
    puts("--- Forest ---");
    for (int i = 0; i < f->nroots; ++i) print_node(f->roots[i], 0);
}

static int cmp_int(const void *a, const void *b) {
    int A = *(const int*)a;
    int B = *(const int*)b;
    return (A > B) - (A < B);
}

static int *normalize_vertices(const int *in, int n_in, int *n_out) {
    if (n_in == 0) { *n_out = 0; return NULL; }
    int *a = malloc(sizeof(int) * n_in);
    if (!a) { perror("malloc"); exit(1); }
    memcpy(a, in, sizeof(int) * n_in);
    qsort(a, n_in, sizeof(int), cmp_int);
    int w = 1;
    for (int i = 1; i < n_in; ++i) if (a[i] != a[w-1]) a[w++] = a[i];
    *n_out = w;
    a = realloc(a, sizeof(int) * w);
    return a;
}

static void insert_hyperedge(Forest *f, const int *verts, int nverts, double weight) {
    int n_norm;
    int *norm = normalize_vertices(verts, nverts, &n_norm);
    Node *nd = node_create(norm, n_norm, weight);
    free(norm);
    forest_insert(f, nd);
}

// Count total nodes in forest
static int count_nodes_recursive(Node *nd) {
    int count = 1;
    for (int i = 0; i < nd->nchildren; ++i) {
        count += count_nodes_recursive(nd->children[i]);
    }
    return count;
}

static int count_total_nodes(Forest *f) {
    int total = 0;
    for (int i = 0; i < f->nroots; ++i) {
        total += count_nodes_recursive(f->roots[i]);
    }
    return total;
}

// Find node with specific vertex set
static Node *find_node_recursive(Node *nd, const int *verts, int nverts) {
    if (nd->he.nverts == nverts) {
        int match = 1;
        for (int i = 0; i < nverts; ++i) {
            if (nd->he.verts[i] != verts[i]) {
                match = 0;
                break;
            }
        }
        if (match) return nd;
    }
    for (int i = 0; i < nd->nchildren; ++i) {
        Node *result = find_node_recursive(nd->children[i], verts, nverts);
        if (result) return result;
    }
    return NULL;
}

static Node *find_node(Forest *f, const int *verts, int nverts) {
    for (int i = 0; i < f->nroots; ++i) {
        Node *result = find_node_recursive(f->roots[i], verts, nverts);
        if (result) return result;
    }
    return NULL;
}

void test_basic() {
    printf("\n=== TEST 1: Basic Insertion ===\n");
    Forest *f = forest_create();
    
    int a[] = {1,2,3};
    int b[] = {1,2};
    
    insert_hyperedge(f, a, 3, 1.0);
    insert_hyperedge(f, b, 2, 0.5);
    
    print_forest(f);
    
    assert(f->nroots == 1);
    assert(f->roots[0]->nchildren == 1);
    printf("✓ Basic parent-child relationship works\n");
    
    forest_free(f);
}

void test_reverse_order() {
    printf("\n=== TEST 2: Reverse Insertion Order ===\n");
    Forest *f = forest_create();
    
    int a[] = {1,2};
    int b[] = {1,2,3};
    
    insert_hyperedge(f, a, 2, 0.5);
    insert_hyperedge(f, b, 3, 1.0);
    
    print_forest(f);
    
    assert(f->nroots == 1);
    assert(f->roots[0]->he.nverts == 3);
    assert(f->roots[0]->nchildren == 1);
    printf("✓ Reverse order insertion works\n");
    
    forest_free(f);
}

void test_incomparable() {
    printf("\n=== TEST 3: Incomparable Sets ===\n");
    Forest *f = forest_create();
    
    int a[] = {1,2};
    int b[] = {3,4};
    int c[] = {5,6,7};
    
    insert_hyperedge(f, a, 2, 1.0);
    insert_hyperedge(f, b, 2, 2.0);
    insert_hyperedge(f, c, 3, 3.0);
    
    print_forest(f);
    
    assert(f->nroots == 3);
    printf("✓ Incomparable sets remain as separate roots\n");
    
    forest_free(f);
}

void test_complex_hierarchy() {
    printf("\n=== TEST 4: Complex Multi-Level Hierarchy ===\n");
    Forest *f = forest_create();
    
    int a[] = {1};
    int b[] = {1,2};
    int c[] = {1,2,3};
    int d[] = {1,2,3,4};
    int e[] = {1,2,3,4,5};
    
    insert_hyperedge(f, c, 3, 3.0);
    insert_hyperedge(f, a, 1, 1.0);
    insert_hyperedge(f, e, 5, 5.0);
    insert_hyperedge(f, b, 2, 2.0);
    insert_hyperedge(f, d, 4, 4.0);
    
    print_forest(f);
    
    assert(f->nroots == 1);
    assert(f->roots[0]->he.nverts == 5);
    printf("✓ Deep hierarchy forms correctly\n");
    
    forest_free(f);
}

void test_siblings() {
    printf("\n=== TEST 5: Sibling Sets (Multiple Children) ===\n");
    Forest *f = forest_create();
    
    int parent[] = {1,2,3,4,5,6};
    int child1[] = {1,2};
    int child2[] = {3,4};
    int child3[] = {5,6};
    
    insert_hyperedge(f, parent, 6, 10.0);
    insert_hyperedge(f, child1, 2, 1.0);
    insert_hyperedge(f, child2, 2, 2.0);
    insert_hyperedge(f, child3, 2, 3.0);
    
    print_forest(f);
    
    assert(f->nroots == 1);
    assert(f->roots[0]->nchildren == 3);
    printf("✓ Multiple disjoint children work correctly\n");
    
    forest_free(f);
}

void test_weight_preservation() {
    printf("\n=== TEST 6: Weight Preservation ===\n");
    Forest *f = forest_create();
    
    int a[] = {1,2,3};
    insert_hyperedge(f, a, 3, 42.5);
    
    int norm_a[] = {1,2,3};
    Node *node = find_node(f, norm_a, 3);
    
    assert(node != NULL);
    assert(node->he.weight == 42.5);
    printf("✓ Weights are preserved correctly (%.2f)\n", node->he.weight);
    
    forest_free(f);
}

void test_duplicate_vertices() {
    printf("\n=== TEST 7: Duplicate Vertex Handling ===\n");
    Forest *f = forest_create();
    
    int a[] = {3,1,2,1,3,2};  // Should normalize to {1,2,3}
    insert_hyperedge(f, a, 6, 1.0);
    
    assert(f->nroots == 1);
    assert(f->roots[0]->he.nverts == 3);
    assert(f->roots[0]->he.verts[0] == 1);
    assert(f->roots[0]->he.verts[1] == 2);
    assert(f->roots[0]->he.verts[2] == 3);
    printf("✓ Duplicate vertices normalized correctly\n");
    
    forest_free(f);
}

void test_large_scale() {
    printf("\n=== TEST 8: Large Scale Stress Test ===\n");
    Forest *f = forest_create();
    
    // Insert 100 random hyperedges
    for (int i = 0; i < 100; ++i) {
        int size = (i % 10) + 1;
        int *verts = malloc(sizeof(int) * size);
        for (int j = 0; j < size; ++j) {
            verts[j] = (i + j) % 50;
        }
        insert_hyperedge(f, verts, size, (double)i);
        free(verts);
    }
    
    int total = count_total_nodes(f);
    printf("Inserted 100 hyperedges, resulted in %d nodes in forest\n", total);
    printf("Number of root trees: %d\n", f->nroots);
    assert(total == 100);
    printf("✓ Large scale insertion successful\n");
    
    forest_free(f);
}

void test_rearrangement() {
    printf("\n=== TEST 9: Dynamic Rearrangement ===\n");
    Forest *f = forest_create();
    
    // Start with two separate trees
    int a[] = {1,2};
    int b[] = {5,6};
    insert_hyperedge(f, a, 2, 1.0);
    insert_hyperedge(f, b, 2, 2.0);
    
    printf("Before unifying parent:\n");
    print_forest(f);
    assert(f->nroots == 2);
    
    // Insert a parent that encompasses both
    int parent[] = {1,2,5,6};
    insert_hyperedge(f, parent, 4, 10.0);
    
    printf("\nAfter unifying parent:\n");
    print_forest(f);
    assert(f->nroots == 1);
    assert(f->roots[0]->nchildren == 2);
    printf("✓ Dynamic tree rearrangement works\n");
    
    forest_free(f);
}

void test_overlapping() {
    printf("\n=== TEST 10: Overlapping Sets (Not Subset) ===\n");
    Forest *f = forest_create();
    
    int a[] = {1,2,3};
    int b[] = {2,3,4};  // Overlaps but neither is subset
    int c[] = {3,4,5};
    
    insert_hyperedge(f, a, 3, 1.0);
    insert_hyperedge(f, b, 3, 2.0);
    insert_hyperedge(f, c, 3, 3.0);
    
    print_forest(f);
    
    assert(f->nroots == 3);
    printf("✓ Overlapping (non-subset) sets remain separate\n");
    
    forest_free(f);
}

int main(void) {
    printf("╔════════════════════════════════════════════╗\n");
    printf("║  HYPEREDGE INCLUSION FOREST TEST SUITE    ║\n");
    printf("╚════════════════════════════════════════════╝\n");
    
    test_basic();
    test_reverse_order();
    test_incomparable();
    test_complex_hierarchy();
    test_siblings();
    test_weight_preservation();
    test_duplicate_vertices();
    test_large_scale();
    test_rearrangement();
    test_overlapping();
    
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  ALL TESTS PASSED ✓✓✓                     ║\n");
    printf("╚════════════════════════════════════════════╝\n");
    
    return 0;
}
