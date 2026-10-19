#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tac.h"
#include "dag.h"
#include "cse.h"

typedef struct {
    Operand name;
    DagNode *node;
} Loc;

typedef struct {
    Dag dag;
    int *needed;
    int *rem;
    Loc *cur;
    int ncur;
    int ccap;
    QList out;
    int next_tid;
    Operand *fin_var;
    DagNode **fin_node;
    int *fin_done;
    int nfin;
} Ctx;

static void die(const char *m)
{
    fprintf(stderr, "optimize: %s\n", m);
    exit(1);
}

static int is_immediate(DagNode *n)
{
    return n->is_leaf && !n->is_read && (n->leaf.kind == A_CONST || n->leaf.kind == A_STR);
}

static DagNode *cur_get(Ctx *c, Operand name)
{
    for (int i = 0; i < c->ncur; i++)
        if (operand_eq(c->cur[i].name, name))
            return c->cur[i].node;
    return NULL;
}

static void cur_set(Ctx *c, Operand name, DagNode *node)
{
    for (int i = 0; i < c->ncur; i++) {
        if (operand_eq(c->cur[i].name, name)) {
            c->cur[i].node = node;
            return;
        }
    }
    if (c->ncur == c->ccap) {
        c->ccap *= 2;
        c->cur = realloc(c->cur, c->ccap * sizeof(Loc));
        if (!c->cur) die("out of memory");
    }
    c->cur[c->ncur].name = name;
    c->cur[c->ncur].node = node;
    c->ncur++;
}

static int holder_count(Ctx *c, DagNode *n)
{
    int k = 0;
    for (int i = 0; i < c->ncur; i++)
        if (c->cur[i].node == n)
            k++;
    return k;
}

static Operand src(Ctx *c, DagNode *n)
{
    if (is_immediate(n))
        return n->leaf;
    for (int i = 0; i < c->ncur; i++)
        if (c->cur[i].node == n)
            return c->cur[i].name;
    fprintf(stderr, "optimize: value has no holder\n");
    if (n->is_leaf)
        return n->leaf;
    return mk_none();
}

static int needs_save(Ctx *c, Operand v)
{
    DagNode *m = cur_get(c, v);
    if (!m || is_immediate(m) || c->rem[m->id] <= 0)
        return 0;
    return holder_count(c, m) == 1;
}

static void emit(Ctx *c, Op op, int relop, Operand a1, Operand a2, Operand res)
{
    qlist_add(&c->out, new_quad(op, relop, a1, a2, res));
}

static Operand fresh_temp(Ctx *c)
{
    return mk_temp(c->next_tid++);
}

static void guard_overwrite(Ctx *c, Operand v)
{
    if (!needs_save(c, v))
        return;
    DagNode *m = cur_get(c, v);
    Operand t = fresh_temp(c);
    emit(c, OP_COPY, REL_NONE, v, mk_none(), t);
    cur_set(c, t, m);
}

static void note_assign(Ctx *c, Operand v, DagNode *n)
{
    cur_set(c, v, n);
    for (int i = 0; i < c->nfin; i++) {
        if (!c->fin_done[i] && operand_eq(c->fin_var[i], v) && c->fin_node[i] == n) {
            c->fin_done[i] = 1;
            c->rem[n->id]--;
        }
    }
}

static void mark(Ctx *c, DagNode *n)
{
    if (c->needed[n->id])
        return;
    c->needed[n->id] = 1;
    if (n->is_leaf)
        return;
    mark(c, n->left);
    if (n->right)
        mark(c, n->right);
}

static void scan_temp(Operand o, int *mx)
{
    if (o.kind != A_TEMP)
        return;
    if (o.val > *mx)
        *mx = (int)o.val;
    int id = atoi(o.name + 1);
    if (id > *mx)
        *mx = id;
}

static void emit_compute(Ctx *c, DagNode *n)
{
    Operand a1 = src(c, n->left);
    Operand a2 = n->right ? src(c, n->right) : mk_none();
    c->rem[n->left->id]--;
    if (n->right)
        c->rem[n->right->id]--;

    Operand dest = mk_none();
    int found = 0;
    for (int i = 0; i < n->nlabels && !found; i++) {
        if (n->labels[i].kind == A_VAR && !needs_save(c, n->labels[i])) {
            dest = n->labels[i];
            found = 1;
        }
    }
    for (int i = 0; i < n->nlabels && !found; i++) {
        if (n->labels[i].kind == A_TEMP) {
            dest = n->labels[i];
            found = 1;
        }
    }
    if (!found)
        dest = fresh_temp(c);

    emit(c, n->op, REL_NONE, a1, a2, dest);
    note_assign(c, dest, n);
}

static void emit_read(Ctx *c, DagNode *n)
{
    Operand x = n->leaf;
    guard_overwrite(c, x);
    emit(c, OP_READ, REL_NONE, mk_none(), mk_none(), x);
    note_assign(c, x, n);
}

static void emit_write(Ctx *c, DagNode *n)
{
    Operand a = src(c, n->left);
    c->rem[n->left->id]--;
    emit(c, OP_WRITE, REL_NONE, a, mk_none(), mk_none());
}

static void emit_finals(Ctx *c)
{
    for (;;) {
        int pick = -1;
        for (int i = 0; i < c->nfin && pick < 0; i++)
            if (!c->fin_done[i] && !needs_save(c, c->fin_var[i]))
                pick = i;
        for (int i = 0; i < c->nfin && pick < 0; i++)
            if (!c->fin_done[i])
                pick = i;
        if (pick < 0)
            return;

        DagNode *n = c->fin_node[pick];
        Operand v = c->fin_var[pick];
        Operand s = src(c, n);
        c->rem[n->id]--;
        c->fin_done[pick] = 1;
        guard_overwrite(c, v);
        emit(c, OP_COPY, REL_NONE, s, mk_none(), v);
        cur_set(c, v, n);
    }
}

Quad *optimize_block(Block *b, int *n)
{
    if (!b || b->n == 0) {
        *n = 0;
        return NULL;
    }

    Ctx c;
    memset(&c, 0, sizeof c);
    dag_init(&c.dag, b->n);
    qlist_init(&c.out);

    Quad *head = malloc(b->n * sizeof(Quad));
    if (!head)
        die("out of memory");
    int nhead = 0;
    Quad tail;
    memset(&tail, 0, sizeof tail);
    int has_tail = 0;
    int maxid = 0;

    for (int i = 0; i < b->n; i++) {
        Quad *q = &b->q[i];
        scan_temp(q->a1, &maxid);
        scan_temp(q->a2, &maxid);
        scan_temp(q->res, &maxid);

        DagNode *l, *r, *x;
        switch (q->op) {
        case OP_LABEL:
            head[nhead++] = *q;
            break;
        case OP_GOTO:
        case OP_IFREL:
            tail = *q;
            has_tail = 1;
            break;
        case OP_COPY:
            x = dag_node_of(&c.dag, q->a1);
            dag_bind(&c.dag, q->res, x);
            break;
        case OP_NEG:
            l = dag_node_of(&c.dag, q->a1);
            x = cse_unary_neg(&c.dag, l);
            dag_bind(&c.dag, q->res, x);
            break;
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
            l = dag_node_of(&c.dag, q->a1);
            r = dag_node_of(&c.dag, q->a2);
            x = cse_binary(&c.dag, q->op, l, r);
            dag_bind(&c.dag, q->res, x);
            break;
        case OP_READ:
            dag_fresh_leaf(&c.dag, q->res);
            break;
        case OP_WRITE:
            x = dag_node_of(&c.dag, q->a1);
            dag_make_write(&c.dag, x);
            break;
        }
    }
    c.next_tid = maxid + 1;

    DagNode *j1 = NULL, *j2 = NULL;
    if (has_tail && tail.op == OP_IFREL) {
        j1 = dag_node_of(&c.dag, tail.a1);
        j2 = dag_node_of(&c.dag, tail.a2);
    }

    Dag *d = &c.dag;
    int nn = d->nnodes;
    c.needed = calloc(nn + 1, sizeof(int));
    c.rem = calloc(nn + 1, sizeof(int));
    c.ccap = d->ntable + nn * 2 + 16;
    c.cur = malloc(c.ccap * sizeof(Loc));
    c.fin_var = malloc((d->ntable + 1) * sizeof(Operand));
    c.fin_node = malloc((d->ntable + 1) * sizeof(DagNode *));
    c.fin_done = calloc(d->ntable + 1, sizeof(int));
    if (!c.needed || !c.rem || !c.cur || !c.fin_var || !c.fin_node || !c.fin_done)
        die("out of memory");

    for (int i = 0; i < nn; i++) {
        DagNode *nd = d->nodes[i];
        if (nd->is_leaf && !nd->is_read && (nd->leaf.kind == A_VAR || nd->leaf.kind == A_TEMP))
            cur_set(&c, nd->leaf, nd);
    }

    for (int i = 0; i < nn; i++) {
        DagNode *nd = d->nodes[i];
        if ((nd->is_leaf && nd->is_read) || nd->is_write)
            mark(&c, nd);
    }

    for (int i = 0; i < d->ntable; i++) {
        Binding *bd = &d->table[i];
        if (bd->name.kind != A_VAR)
            continue;
        DagNode *nd = bd->node;
        if (nd->is_leaf && !nd->is_read && operand_eq(nd->leaf, bd->name))
            continue;
        c.fin_var[c.nfin] = bd->name;
        c.fin_node[c.nfin] = nd;
        c.nfin++;
        mark(&c, nd);
    }

    if (j1) {
        mark(&c, j1);
        mark(&c, j2);
    }

    for (int i = 0; i < nn; i++) {
        DagNode *nd = d->nodes[i];
        if (!c.needed[nd->id] || nd->is_leaf)
            continue;
        c.rem[nd->left->id]++;
        if (nd->right)
            c.rem[nd->right->id]++;
    }
    for (int i = 0; i < c.nfin; i++)
        c.rem[c.fin_node[i]->id]++;
    if (j1) {
        c.rem[j1->id]++;
        c.rem[j2->id]++;
    }

    for (int i = 0; i < nhead; i++)
        qlist_add(&c.out, head[i]);

    for (int i = 0; i < nn; i++) {
        DagNode *nd = d->nodes[i];
        if (!c.needed[nd->id])
            continue;
        if (nd->is_leaf) {
            if (nd->is_read)
                emit_read(&c, nd);
        } else if (nd->is_write) {
            emit_write(&c, nd);
        } else {
            emit_compute(&c, nd);
        }
    }

    emit_finals(&c);

    if (has_tail) {
        if (tail.op == OP_IFREL) {
            Operand s1 = src(&c, j1);
            Operand s2 = src(&c, j2);
            emit(&c, OP_IFREL, tail.relop, s1, s2, tail.res);
        } else {
            qlist_add(&c.out, tail);
        }
    }

    *n = c.out.n;
    Quad *result = c.out.q;

    free(head);
    free(c.needed);
    free(c.rem);
    free(c.cur);
    free(c.fin_var);
    free(c.fin_node);
    free(c.fin_done);
    dag_free(&c.dag);
    return result;
}