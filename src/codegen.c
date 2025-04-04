#include <assert.h>
#include <stdio.h>

#include "5cc.h"

static int depth;

static char *argreg8[] = {"%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"};
static char *argreg16[] = {"%di", "%si", "%dx", "%cx", "%r8w", "%r9w"};
static char *argreg32[] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
static char *argreg64[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

static Obj *current_fn;
static FILE *output_file;

static void println(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(output_file, fmt, ap);
    va_end(ap);
    fprintf(output_file, "\n");
    return ;
}

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

int align_to(int n, int align) {
    return (n + align - 1) / align * align;
}

static void load(Type *type) {
    if (type->kind == TY_ARRAY || type->kind == TY_STRUCT || type->kind == TY_UNION)
        return;
    if (type->size == 1)
        println("\tmovsbq (%%rax), %%rax");
    else if (type->size == 2)
        println("\tmovswq (%%rax), %%rax");
    else if (type->size == 4)
        println("\tmovsxd (%%rax), %%rax");
    else
        println("\tmov (%%rax), %%rax");
    return;
}

static void store(Type *type) {
    pop("%rdi");
    if (type->kind == TY_STRUCT || type->kind == TY_UNION) {
        for (int i = 0; i < type->size; i++) {
            println("\tmov %d(%%rax), %%r8b", i);
            println("\tmov %%r8b, %d(%%rdi)", i);
        }
        return;
    }
    if (type->size == 1)
        println("\tmov %%al, (%%rdi)");
    else if (type->size == 2)
        println("\tmov %%ax, (%%rdi)");
    else if (type->size == 4)
        println("\tmov %%eax, (%%rdi)");
    else
        println("\tmov %%rax, (%%rdi)");
}

static void gen_stmt(Node *node);
static void gen_expr(Node *node);

static void gen_addr(Node *node) {
    switch (node->kind) {
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
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_addr(node->rhs);
        return;
    }
    Error("not an lvalue");
}

static void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        println("\tmov $%ld, %%rax", node->val);
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
    case ND_STMT_EXPR:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
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

    char *ax, *di;

    if (node->lhs->type->kind == TY_LONG || node->lhs->type->base) {
        ax = "%rax";
        di = "%rdi";
    } else {
        ax = "%eax";
        di = "%edi";
    }

    switch (node->kind) {
    case ND_ADD:
        println("\tadd %s, %s", di, ax);
        return;
    case ND_SUB:
        println("\tsub %s, %s", di, ax);
        return;
    case ND_MUL:
        println("\timul %s, %s", di, ax);
        return;
    case ND_DIV:
        if (node->lhs->type->size == 8)
            println("\tcqo");
        else
            println("\tcdq");
        println("\tidiv %s", di);   
        return;
    case ND_MOD:
        if (node->lhs->type->size == 8) {
            println("\tcqo");
            println("\tidiv %s", di); 
            println("\tmov %%rdx, %s", ax);
        } else {
            println("\tcdq");
            println("\tidiv %s", di); 
            println("\tmov %%edx, %s", ax);
        }
        return;
    case ND_AND:
        println("and(%s, %s)", di, ax);
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LE:
    case ND_LT:
        println("\tcmp %s, %s", di, ax);
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
        offset = align_to(offset, lv->type->align);
        lv->offset = -offset;
    }
    func->stack_size = align_to(offset, 16);
}

static void store_param(int r, int offset, int size) {
    switch (size) {
    case 1:
        println("\tmov %s, %d(%%rbp)", argreg8[r], offset);
        return;
    case 2:
        println("\tmov %s, %d(%%rbp)", argreg16[r], offset);
        return;
    case 4:
        println("\tmov %s, %d(%%rbp)", argreg32[r], offset);
        return;
    case 8:
        println("\tmov %s, %d(%%rbp)", argreg64[r], offset);
        return;
  }
  Error("something is wrong");
}

static void EmitData(Obj* gvar) {
    for (Obj *var = gvar; var; var = var->next) {
        if (var->is_func) continue;

        println(".data");
        println("\t.global %s", var->name);
        println("%s:", var->name);
        
        if (var->init_data) {
            for (int i = 0; i < var->type->array_len; i++)
                println("\t.byte %d", var->init_data[i]);
        } else {
            println("\t.zero %d", var->type->size);
        }
    }
}

static void EmitFunc(Obj *func) {
    for (Obj *fn = func; fn; fn = fn->next) {
        if (!fn->is_func || !fn->is_def) continue;
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
            store_param(i++, var->offset, var->type->size);
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

void GenCode(Obj *prog, FILE *out) {
    output_file = out;

    EmitData(prog);
    EmitFunc(prog);
}