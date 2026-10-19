#include <stdio.h>
#include <stdlib.h>
#include "dag.h"

static void die(const char *msg)
{
    fprintf(stderr, "dag: %s\n", msg);
    exit(1);
}

static DagNode *new_node(Dag *d)
{
    if (d->nnodes == d->ncap) {
        d->ncap *= 2;
        d->nodes = realloc(d->nodes, d->ncap * sizeof(DagNode *));
        if (!d->nodes) die("out of memory");
    }
    DagNode *n = calloc(1, sizeof(DagNode));
    if (!n) die("out of memory");
    n->id = d->nnodes;
    n->seq = d->next_seq++;
    d->nodes[d->nnodes++] = n;
    return n;
}

static Binding *find_binding(Dag *d, Operand name)
{
    for (int i = 0; i < d->ntable; i++)
        if (operand_eq(d->table[i].name, name))
            return &d->table[i];
    return NULL;
}

static void add_binding(Dag *d, Operand name, DagNode *n)
{
    if (d->ntable == d->tcap) {
        d->tcap *= 2;
        d->table = realloc(d->table, d->tcap * sizeof(Binding));
        if (!d->table) die("out of memory");
    }
    Binding *b = &d->table[d->ntable++];
    b->name = name;
    b->node = n;
    b->bind_seq = d->next_seq++;
}

static void add_label(DagNode *n, Operand name)
{
    if (n->nlabels == MAX_LABELS) die("too many labels on one node");
    n->labels[n->nlabels++] = name;
}

static void remove_label(DagNode *n, Operand name)
{
    for (int i = 0; i < n->nlabels; i++) {
        if (operand_eq(n->labels[i], name)) {
            for (int j = i; j < n->nlabels - 1; j++)
                n->labels[j] = n->labels[j + 1];
            n->nlabels--;
            return;
        }
    }
}

void dag_init(Dag *d, int nquads)
{
    d->ncap = nquads * 3 + 4;
    d->tcap = nquads * 3 + 4;
    d->nodes = malloc(d->ncap * sizeof(DagNode *));
    d->table = malloc(d->tcap * sizeof(Binding));
    if (!d->nodes || !d->table) die("out of memory");
    d->nnodes = 0;
    d->ntable = 0;
    d->next_seq = 0;
}

void dag_free(Dag *d)
{
    for (int i = 0; i < d->nnodes; i++)
        free(d->nodes[i]);
    free(d->nodes);
    free(d->table);
    d->nodes = NULL;
    d->table = NULL;
    d->nnodes = 0;
    d->ntable = 0;
}

DagNode *dag_const_leaf(Dag *d, long val)
{
    for (int i = 0; i < d->nnodes; i++) {
        DagNode *n = d->nodes[i];
        if (n->is_leaf && !n->is_read && n->leaf.kind == A_CONST && n->leaf.val == val)
            return n;
    }
    DagNode *n = new_node(d);
    n->is_leaf = 1;
    n->leaf = mk_const(val);
    return n;
}

DagNode *dag_node_of(Dag *d, Operand name)
{
    if (name.kind == A_CONST)
        return dag_const_leaf(d, name.val);
    Binding *b = find_binding(d, name);
    if (b)
        return b->node;
    DagNode *n = new_node(d);
    n->is_leaf = 1;
    n->leaf = name;
    add_binding(d, name, n);
    return n;
}

DagNode *dag_fresh_leaf(Dag *d, Operand name)
{
    Binding *b = find_binding(d, name);
    if (b)
        remove_label(b->node, name);
    DagNode *n = new_node(d);
    n->is_leaf = 1;
    n->is_read = 1;
    n->leaf = name;
    if (b) {
        b->node = n;
        b->bind_seq = d->next_seq++;
    } else {
        add_binding(d, name, n);
    }
    return n;
}

DagNode *dag_find_or_make(Dag *d, Op op, DagNode *l, DagNode *r)
{
    for (int i = 0; i < d->nnodes; i++) {
        DagNode *n = d->nodes[i];
        if (!n->is_leaf && !n->is_write && n->op == op && n->left == l && n->right == r) {
            stats.cse++;
            return n;
        }
    }
    DagNode *n = new_node(d);
    n->op = op;
    n->left = l;
    n->right = r;
    return n;
}

DagNode *dag_make_write(Dag *d, DagNode *v)
{
    DagNode *n = new_node(d);
    n->op = OP_WRITE;
    n->is_write = 1;
    n->left = v;
    return n;
}

void dag_bind(Dag *d, Operand name, DagNode *n)
{
    Binding *b = find_binding(d, name);
    if (b) {
        if (b->node == n)
            return;
        remove_label(b->node, name);
        b->node = n;
        b->bind_seq = d->next_seq++;
    } else {
        add_binding(d, name, n);
    }
    add_label(n, name);
}