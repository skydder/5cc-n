#include <assert.h>
#include <stdio.h>

#include "5cc.h"

static int depth;
static char *argreg64[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"}; 
static char *argreg8[] = {"%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"};
static Obj *current_fn;

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

static void load(Type *type) {
    if (type->kind == TY_ARRAY)
        return;
    if (type->size == 1)
        println("\tmovsbq (%%rax), %%rax");
    else
        println("\tmov (%%rax), %%rax");
    return;
}

static void store(Type *type) {
    pop("%rdi");
    if (type->size == 1)
        println("\tmov %%al, (%%rdi)");
    else
        println("\tmov %%rax, (%%rdi)");
}

static void gen_expr(Node *node);
static void gen_addr(Node *node) {
    switch (node->kind ) {
    case ND_VAR:
        if (node->var->is_lvar) {
            println("\tlea %d(%%rbp), %%rax", node->var->offset);
        } else {
            println("\tlea %s(%%rip), %%rax", node->var->name);
        }
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    case ND_DOTS:
        gen_addr(node->lhs);
        println("\tadd $%d, %%rax", node->member->offset);
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
        load(node->type);
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        store(node->type);
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        load(node->type);
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_expr(node->rhs);
        return;
    case ND_DOTS:
        gen_addr(node);
        load(node->type);
        return;
    case ND_FNCALL:{
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            push();
            nargs++;
        }
        
        for (int i = nargs - 1; i >= 0; i--)
            pop(argreg64[i]);

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
        println("\tjmp .L.return.%s", current_fn->name);
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
            gen_stmt(node->init);
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

static void InitLVarOffset(Obj *func) {
    int offset = 0;
    for (Obj *lv = func->locals; lv; lv = lv->next) {
        offset += lv->type->size;
        lv->offset = -offset;
    }
    func->stack_size = align_to(offset, 16);
}

static void EmitData(Obj* gvar) {
    for (Obj *var = gvar; var; var = var->next) {
        if (var->is_func) continue;

        println(".data");
        println("\t.global %s", var->name);
        println("%s:", var->name);
        
        if (gvar->init_data) {
            for (int i = 0; i < gvar->type->array_len; i++)
                println("\t.byte %d", gvar->init_data[i]);
        } else {
            println("\t.zero %d", var->type->size);
        }
    }
}

static void EmitFunc(Obj *func) {
    for (Obj *fn = func; fn; fn = fn->next) {
        if (!fn->is_func) continue;
        assert(fn->is_func);
        InitLVarOffset(fn);
        current_fn = fn;
        println(".text");
        println("\t.globl %s", fn->name);
        
        println("%s:", fn->name);

        println("\tpush %%rbp");
        println("\tmov %%rsp, %%rbp");
        println("\tsub $%d, %%rsp", fn->stack_size);

        int i = 0;
        for (Obj *var = fn->params; var; var = var->next) {
            if (var->type->size == 1)
                println("\tmov %s, %d(%%rbp)\n", argreg8[i++], var->offset);
            else
                println("\tmov %s, %d(%%rbp)", argreg64[i++], var->offset);
        }

        for (Node *n = fn->body; n; n = n->next) {
            gen_stmt(n);
            assert(depth == 0);
        }
        println(".L.return.%s:", fn->name);
        println("\tmov %%rbp, %%rsp");
        println("\tpop %%rbp");
        println("\tret");
    }
}

void GenCode(Obj *prog) {
    EmitData(prog);
    EmitFunc(prog);

}