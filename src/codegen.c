#include <assert.h>
#include <stdio.h>

#include "5cc.h"

static int depth;
static char* argreg[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"}; 

static void comment(char *msg) {
    putchar('#');
    println(msg);
}

static void push(void) {
  println("\tpush %%rax");
  depth++;
}

static void pop(char *arg) {
  println("\tpop %s", arg);
  depth--;
}

static int count() {
    static int i = 1;
    return i++;
}

static int align_to(int n, int align) {
    return (n + align - 1) / align * align;
}
static void gen_expr(Node *node);
static void gen_addr(Node *node) {
    switch (node->kind ) {
    case ND_VAR:
        println("\tlea %d(%%rbp), %%rax", node->var->offset);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    }
    Error("not an lvalue");
}

static void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        comment("num");
        println("\tmov $%d, %%rax", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        println("\tneg %%rax");
        return;
    case ND_VAR:
        comment("var");
        gen_addr(node);
        println("\tmov (%%rax), %%rax");
        return;
    case ND_ASSIGN:
        comment("assign");
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        pop("%rdi");
        println("\tmov %%rax, (%%rdi)");
        return;
    case ND_ADDR:
        comment("addr");
        gen_addr(node->lhs);
        return;
    case ND_DEREF:
        comment("deref");
        gen_expr(node->lhs);
        println("\tmov (%%rax), %%rax");
        return;
    case ND_FNCALL:{
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            push();
            nargs++;
        }
        
        for (int i = nargs - 1; i >= 0; i--)
            pop(argreg[i]);

        println("\tmov $0, %%rax");
        println("\tcall %s", node->fn_name);
        return;
    }
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
    switch (node->kind) {
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    case ND_RETURN:
        gen_expr(node->lhs);
        println("\tjmp .L.return");
        return;
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
        return;
    case ND_IF:{
        int c = count();
        gen_expr(node->cond);
        println("\tcmp $0, %%rax");
        println("\tje  .L.else.%d", c);
        gen_stmt(node->then);
        println("\tjmp .L.end.%d", c);
        println(".L.else.%d:", c);
        if (node->_else)
            gen_stmt(node->_else);
        println(".L.end.%d:", c);
        return;
    }
    case ND_FOR:{
        int c = count();
        if (node->init)
            gen_expr(node->init);
        println(".L.begin.%d:", c);
        if (node->cond) {
            gen_expr(node->cond);
            println("\tcmp $0, %%rax");
            println("\tje  .L.end.%d", c);
        }
        
        gen_stmt(node->then);
        if (node->inc)
            gen_expr(node->inc);
        println("\tjmp .L.begin.%d", c);
        println(".L.end.%d:", c);
        return;
    }
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
    assert(func->is_func);
    Init(func);
    println(".globl main");
    println("main:");

    println("\tpush %%rbp");
    println("\tmov %%rsp, %%rbp");
    println("\tsub $%d, %%rsp", func->stack_size);
    for (Node *n = func->body; n; n = n->next) {
        gen_stmt(n);
        assert(depth == 0);
    }
    println(".L.return:");
    println("\tmov %%rbp, %%rsp");
    println("\tpop %%rbp");
    println("\tret");
    
}