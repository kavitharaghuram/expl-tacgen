#include "cse.h"
#include "fold.h"

int cse_is_const(DagNode *n)
{
    return n->is_leaf && !n->is_read && n->leaf.kind == A_CONST;
}

DagNode *cse_binary(Dag *d, Op op, DagNode *l, DagNode *r)
{
    long res;
    if (cse_is_const(l) && cse_is_const(r) && fold_const(op, l->leaf.val, r->leaf.val, &res))
        return dag_const_leaf(d, res);
    return dag_find_or_make(d, op, l, r);
}

DagNode *cse_unary_neg(Dag *d, DagNode *x)
{
    long res;
    if (cse_is_const(x) && fold_const(OP_SUB, 0, x->leaf.val, &res))
        return dag_const_leaf(d, res);
    return dag_find_or_make(d, OP_NEG, x, NULL);
}