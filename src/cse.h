#ifndef CSE_H
#define CSE_H

#include "dag.h"

int cse_is_const(DagNode *n);
DagNode *cse_binary(Dag *d, Op op, DagNode *l, DagNode *r);
DagNode *cse_unary_neg(Dag *d, DagNode *x);

#endif