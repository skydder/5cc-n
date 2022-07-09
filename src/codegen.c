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

static void gen_addr(Node *node) {
    if (node->kind == ND_VAR) {
        int offset = (node->name - 'a' + 1) * 8;
        println("\tlea %d(%%rbp), %%rax", -offset);
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

void GenCode(Node *node) {
    println(".globl main");
    println("main:");

    println("\tpush %%rbp");
    println("\tmov %%rsp, %%rbp");
    println("\tsub $208, %%rsp");
    for (Node *n = node; n; n = n->next) {
        gen_stmt(n);
        assert(depth == 0);
    }
    println("\tmov %%rbp, %%rsp");
    println("\tpop %%rbp");
    println("\tret");
    
}