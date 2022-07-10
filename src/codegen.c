#include <assert.h>

#include "5cc.h"

static int depth;

static void push(void) {
  println("\tpush %%rax");
  depth++;
}

static void pop(char *arg) {
  println("\tpop %s", arg);
  depth--;
}

static int align_to(int n, int align) {
    return (n + align - 1) / align * align;
}

static void gen_addr(Node *node) {
    if (node->kind == ND_VAR) {
        println("\tlea %d(%%rbp), %%rax", node->var->offset);
        return;
    }
    Error("not an lvalue");
}

static void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        println("\tmov $%d, %%rax", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        println("\tneg %%rax");
        return;
    case ND_VAR:
        gen_addr(node);
        println("\tmov (%%rax), %%rax");
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        pop("%rdi");
        println("\tmov %%rax, (%%rdi)");
        return;
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("%rdi");

    switch (node->kind) {
    case ND_ADD:
        println("\tadd %%rdi, %%rax");
        return;
    case ND_SUB:
        println("\tsub %%rdi, %%rax");
        return;
    case ND_MUL:
        println("\timul %%rdi, %%rax");
        return;
    case ND_DIV:
        println("\tcqo");
        println("\tidiv %%rdi");
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LE:
    case ND_LT:
        println("\tcmp %%rdi, %%rax");
        if (node->kind == ND_EQ) {
            println("\tsete %%al");
        } else if (node->kind == ND_NE) {
            println("\tsetne %%al");
        } else if (node->kind == ND_LT) {
            println("\tsetl %%al");
        } else if (node->kind == ND_LE) {
            println("\tsetle %%al");
        }
        println("\tmovzb %%al, %%rax");
        return;
    }

    Error("invalid expression");
}

static void gen_stmt(Node *node) {
    if (node->kind == ND_EXPR_STMT) {
        gen_expr(node->lhs);
        return;
    }
    Error("invalid expression");
}

void Init(Obj *func) {
    int offset = 0;
    for (Obj *lv = func->locals; lv; lv = lv->next) {
        offset += 8;
        lv->offset = -offset;
    }
    func->stack_size = align_to(offset, 16);
}

void GenCode(Obj *func) {
    Init(func);
    println(".globl main");
    println("main:");

    println("\tpush %%rbp");
    println("\tmov %%rsp, %%rbp");
    println("\tsub $%d, %%rsp", func->stack_size);
    for (Node *n = func->prog; n; n = n->next) {
        gen_stmt(n);
        assert(depth == 0);
    }
    println("\tmov %%rbp, %%rsp");
    println("\tpop %%rbp");
    println("\tret");
    
}