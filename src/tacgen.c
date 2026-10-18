#include "tacgen.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static QList ql;

/* Label stacks for break and continue */
#define MAX_LOOP_DEPTH 64
static char loop_begin_stack[MAX_LOOP_DEPTH][32];
static char loop_end_stack[MAX_LOOP_DEPTH][32];
static int loop_depth = 0;

static void push_loop(const char *lbegin, const char *lend) {
    if (loop_depth < MAX_LOOP_DEPTH) {
        strncpy(loop_begin_stack[loop_depth], lbegin, 31);
        strncpy(loop_end_stack[loop_depth], lend, 31);
        loop_depth++;
    }
}

static void pop_loop(void) {
    if (loop_depth > 0) loop_depth--;
}

static Operand emit_expr(Expr *e);
static void emit_cond(Expr *e, const char *label_true, const char *label_false);
static void emit_stmt(Stmt *s);

static RelOp get_relop(ExprOp op) {
    switch (op) {
        case AST_LT: return REL_LT;
        case AST_GT: return REL_GT;
        case AST_LE: return REL_LE;
        case AST_GE: return REL_GE;
        case AST_EQ: return REL_EQ;
        case AST_NE: return REL_NE;
        default:     return REL_NONE;
    }
}

static Op get_arith_op(ExprOp op) {
    switch (op) {
        case AST_ADD: return OP_ADD;
        case AST_SUB: return OP_SUB;
        case AST_MUL: return OP_MUL;
        case AST_DIV: return OP_DIV;
        case AST_MOD: return OP_MOD;
        default:      return OP_ADD;
    }
}

/* Postorder evaluation of arithmetic and variable expressions (ALSU 6.2.1) */
static Operand emit_expr(Expr *e) {
    if (!e) return mk_none();

    switch (e->kind) {
        case EXPR_INT:
            return mk_const(e->data.int_value);

        case EXPR_STRING:
            return mk_str(e->data.string_value);

        case EXPR_VAR:
            return mk_var(e->data.var_name);

        case EXPR_UNARY:
            if (e->data.unary.op == AST_NEG) {
                Operand op1 = emit_expr(e->data.unary.operand);
                Operand res = mk_var(new_temp());
                qlist_add(&ql, new_quad(OP_NEG, REL_NONE, op1, mk_none(), res));
                return res;
            }
            break;

        case EXPR_BINARY: {
            ExprOp op = e->data.binary.op;
            if (op == AST_ADD || op == AST_SUB || op == AST_MUL || op == AST_DIV || op == AST_MOD) {
                Operand op1 = emit_expr(e->data.binary.left);
                Operand op2 = emit_expr(e->data.binary.right);
                Operand res = mk_var(new_temp());
                qlist_add(&ql, new_quad(get_arith_op(op), REL_NONE, op1, op2, res));
                return res;
            }
            break;
        }
    }
    return mk_none();
}

/* Jumping code translation for boolean conditions (ALSU 6.6.5) */
static void emit_cond(Expr *e, const char *label_true, const char *label_false) {
    if (!e) return;

    if (e->kind == EXPR_UNARY && e->data.unary.op == AST_NOT) {
        emit_cond(e->data.unary.operand, label_false, label_true);
        return;
    }

    if (e->kind == EXPR_BINARY) {
        ExprOp op = e->data.binary.op;
        if (op == AST_AND) {
            char *next_label = strdup(new_label());
            emit_cond(e->data.binary.left, next_label, label_false);
            qlist_add(&ql, new_quad(OP_LABEL, REL_NONE, mk_none(), mk_none(), mk_var(next_label)));
            emit_cond(e->data.binary.right, label_true, label_false);
            free(next_label);
            return;
        }
        if (op == AST_OR) {
            char *next_label = strdup(new_label());
            emit_cond(e->data.binary.left, label_true, next_label);
            qlist_add(&ql, new_quad(OP_LABEL, REL_NONE, mk_none(), mk_none(), mk_var(next_label)));
            emit_cond(e->data.binary.right, label_true, label_false);
            free(next_label);
            return;
        }
        RelOp rop = get_relop(op);
        if (rop != REL_NONE) {
            Operand op1 = emit_expr(e->data.binary.left);
            Operand op2 = emit_expr(e->data.binary.right);
            qlist_add(&ql, new_quad(OP_IFREL, rop, op1, op2, mk_var(label_true)));
            qlist_add(&ql, new_quad(OP_GOTO, REL_NONE, mk_none(), mk_none(), mk_var(label_false)));
            return;
        }
    }
}

/* Statement translation (ALSU 6.6) */
static void emit_stmt(Stmt *s) {
    while (s) {
        switch (s->kind) {
            case STMT_ASSIGN: {
                Operand rval = emit_expr(s->data.assign.value);
                qlist_add(&ql, new_quad(OP_COPY, REL_NONE, rval, mk_none(), mk_var(s->data.assign.name)));
                break;
            }
            case STMT_READ:
                qlist_add(&ql, new_quad(OP_READ, REL_NONE, mk_none(), mk_none(), mk_var(s->data.read_name)));
                break;

            case STMT_WRITE: {
                Operand val = emit_expr(s->data.write_expr);
                qlist_add(&ql, new_quad(OP_WRITE, REL_NONE, val, mk_none(), mk_none()));
                break;
            }
            case STMT_IF: {
                char *l_true = strdup(new_label());
                char *l_false = strdup(new_label());
                char *l_end = (s->data.if_stmt.else_body) ? strdup(new_label()) : l_false;

                emit_cond(s->data.if_stmt.condition, l_true, l_false);

                qlist_add(&ql, new_quad(OP_LABEL, REL_NONE, mk_none(), mk_none(), mk_var(l_true)));
                emit_stmt(s->data.if_stmt.then_body);

                if (s->data.if_stmt.else_body) {
                    qlist_add(&ql, new_quad(OP_GOTO, REL_NONE, mk_none(), mk_none(), mk_var(l_end)));
                    qlist_add(&ql, new_quad(OP_LABEL, REL_NONE, mk_none(), mk_none(), mk_var(l_false)));
                    emit_stmt(s->data.if_stmt.else_body);
                    qlist_add(&ql, new_quad(OP_LABEL, REL_NONE, mk_none(), mk_none(), mk_var(l_end)));
                    free(l_end);
                } else {
                    qlist_add(&ql, new_quad(OP_LABEL, REL_NONE, mk_none(), mk_none(), mk_var(l_false)));
                }

                free(l_true);
                free(l_false);
                break;
            }
            case STMT_WHILE: {
                char *l_begin = strdup(new_label());
                char *l_body = strdup(new_label());
                char *l_end = strdup(new_label());

                qlist_add(&ql, new_quad(OP_LABEL, REL_NONE, mk_none(), mk_none(), mk_var(l_begin)));
                emit_cond(s->data.while_stmt.condition, l_body, l_end);

                qlist_add(&ql, new_quad(OP_LABEL, REL_NONE, mk_none(), mk_none(), mk_var(l_body)));
                push_loop(l_begin, l_end);
                emit_stmt(s->data.while_stmt.body);
                pop_loop();

                qlist_add(&ql, new_quad(OP_GOTO, REL_NONE, mk_none(), mk_none(), mk_var(l_begin)));
                qlist_add(&ql, new_quad(OP_LABEL, REL_NONE, mk_none(), mk_none(), mk_var(l_end)));

                free(l_begin);
                free(l_body);
                free(l_end);
                break;
            }
            case STMT_BREAK:
                if (loop_depth > 0) {
                    qlist_add(&ql, new_quad(OP_GOTO, REL_NONE, mk_none(), mk_none(), mk_var(loop_end_stack[loop_depth - 1])));
                }
                break;

            case STMT_CONTINUE:
                if (loop_depth > 0) {
                    qlist_add(&ql, new_quad(OP_GOTO, REL_NONE, mk_none(), mk_none(), mk_var(loop_begin_stack[loop_depth - 1])));
                }
                break;

            case STMT_RETURN:
                /* ExpL return statement inside main: ignored in TAC as agreed */
                break;
        }
        s = s->next;
    }
}

Quad *gen_tac(Program *prog, int *n) {
    qlist_init(&ql);
    reset_counters();
    loop_depth = 0;

    if (prog) {
        emit_stmt(prog->statements);
    }

    *n = ql.n;
    return ql.q;
}
