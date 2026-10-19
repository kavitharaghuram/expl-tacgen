#ifndef DAG_H
#define DAG_H

#include "tac.h"

#define MAX_LABELS 32

typedef struct DagNode {
    int id;
    int seq;
    int is_leaf;
    int is_read;
    int is_write;
    Operand leaf;
    Op op;
    struct DagNode *left;
    struct DagNode *right;
    Operand labels[MAX_LABELS];
    int nlabels;
    int emitted;
} DagNode;

typedef struct {
    Operand name;
    DagNode *node;
    int bind_seq;
} Binding;

typedef struct {
    DagNode **nodes;
    int nnodes;
    int ncap;
    Binding *table;
    int ntable;
    int tcap;
    int next_seq;
} Dag;

void dag_init(Dag *d, int nquads);
void dag_free(Dag *d);
DagNode *dag_node_of(Dag *d, Operand name);
DagNode *dag_const_leaf(Dag *d, long val);
DagNode *dag_fresh_leaf(Dag *d, Operand name);
DagNode *dag_find_or_make(Dag *d, Op op, DagNode *l, DagNode *r);
DagNode *dag_make_write(Dag *d, DagNode *v);
void dag_bind(Dag *d, Operand name, DagNode *n);

#endif