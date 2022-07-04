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

void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        println("\tmov $%d, %%rax", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        println("\tneg %%rax");
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