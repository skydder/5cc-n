#include <assert.h>
#include <stdio.h>

#include "5cc.h"

static int depth;

static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

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

static void print(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(output_file, fmt, ap);
    va_end(ap);
    return ;
}

static void comment(char *msg) {
    print("(");
    println(msg);
}

static void push(void) {
  println("\tpush rax");
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
    // comment("gen_load");
    if (type->kind == TY_ARRAY || type->kind == TY_STRUCT || type->kind == TY_UNION)
        return;
    if (type->size == 1)
        println("\tmove *(rax by 8bit) to rax with sign-extention");
    else if (type->size == 2)
        println("\tmove *(rax by 16bit) to rax with sign-extention");
    else if (type->size == 4)
        println("\tmove *(rax by 32bit) to rax with sign-extention");
    else
        println("\tmove *(rax) to rax");
    return;
}

static void store(Type *type) {
    // comment("gen_store");
    pop("rdi");
    if (type->kind == TY_STRUCT || type->kind == TY_UNION) {
        for (int i = 0; i < type->size; i++) {
            println("\tmove *(rax + %d) to r8b", i);
            println("\tmove r8b to *(rdi + %d)", i);
        }
        return;
    }
    if (type->size == 1)
        println("\tmove al to *(rdi)");
    else if (type->size == 2)
        println("\tmove ax to *(rdi)");
    else if (type->size == 4)
        println("\tmove eax to *(rdi)");
    else
        println("\tmove rax to *(rdi)");
}

static void gen_stmt(Node *node);
static void gen_expr(Node *node);

static void gen_addr(Node *node) {
    // comment("gen_addr");
    switch (node->kind) {
    case ND_VAR:
        if (node->var->is_lvar) {
            println("\tload-effective-address *(rbp-%d) to rax", node->var->offset);
        } else {
            // println("\tload-effective-address *(rip+%s) to rax", node->var->name);
            println("\tmove %s to rax", node->var->name);
            // println("\tlea %s(%%rip), %%rax", node->var->name);
        }
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    case ND_DOTS:
        gen_addr(node->lhs);
        println("\tadd %d to rax", node->member->offset);
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_addr(node->rhs);
        return;
    }
    Error("not an lvalue");
}

static void gen_expr(Node *node) {
    // comment("gen_expr");
    switch (node->kind) {
    case ND_NUM:
        println("\tmove %ld to rax", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        println("\tnegate rax");
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

        println("\tmove 0 to rax");
        println("\tcall %s", node->fn_name);
        return;
    }
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("rdi");

    char *ax, *di;

    if (node->lhs->type->kind == TY_LONG || node->lhs->type->base) {
        ax = "rax";
        di = "rdi";
    } else {
        ax = "eax";
        di = "edi";
    }

    switch (node->kind) {
    case ND_ADD:
        println("\tadd %s to %s", di, ax);
        return;
    case ND_SUB:
        println("\tsubstract %s from %s", di, ax);
        return;
    case ND_MUL:
        println("\tmultiply %s by %s as signed", ax, di);
        return;
    case ND_DIV:
        if (node->lhs->type->size == 8)
            println("\textend-*ax-reg by 64bit");
        else
            println("\textend-*ax-reg by 32bit");
        println("\tdivide by %s as signed", di);   
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LE:
    case ND_LT:
        println("\tcompare %s with %s", di, ax);
        if (node->kind == ND_EQ) {
            println("\tset-byte to al if ==");
        } else if (node->kind == ND_NE) {
            println("\tset-byte to al if !=");
        } else if (node->kind == ND_LT) {
            println("\tset-byte to al if <");
        } else if (node->kind == ND_LE) {
            println("\tset-byte to al if <=");
        }
        println("\tmove al to rax with sign-extention");
        return;
    }

    Error("invalid expression");
}

static void gen_stmt(Node *node) {
    // comment("gen_stmt");
    switch (node->kind) {
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    case ND_RETURN:
        gen_expr(node->lhs);
        println("\tjump to .L.return.%s", current_fn->name);
        return;
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
        return;
    case ND_IF:{
        int c = count();
        gen_expr(node->cond);
        println("\tcompare 0 with rax");
        println("\tjump to .L.else.%d if ==", c);
        gen_stmt(node->then);
        println("\tjump to .L.end.%d", c);
        println("#.L.else.%d", c);
        if (node->_else)
            gen_stmt(node->_else);
        println("#.L.end.%d", c);
        return;
    }
    case ND_FOR:{
        int c = count();
        if (node->init)
            gen_stmt(node->init);
        println("#.L.begin.%d", c);
        if (node->cond) {
            gen_expr(node->cond);
            println("\tcompare 0 with rax");
            println("\tjump to .L.end.%d if ==", c);
        }
        
        gen_stmt(node->then);
        if (node->inc)
            gen_expr(node->inc);
        println("\tjump to .L.begin.%d", c);
        println("#.L.end.%d", c);
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
        // lv->offset = -offset;
        lv->offset = offset;
    }
    func->stack_size = align_to(offset, 16);
}

static void store_param(int r, int offset, int size) {
    comment("storing paramater");
    switch (size) {
    case 1:
        // println("\tmov %s, %d(%%rbp)", argreg8[r], offset);
        println("\tmove %s to *(rbp - %d)", argreg8[r], offset);
        return;
    case 2:
        // println("\tmov %s, %d(%%rbp)", argreg16[r], offset);
        println("\tmove %s to *(rbp - %d)", argreg16[r], offset);
        return;
    case 4:
        // println("\tmov %s, %d(%%rbp)", argreg32[r], offset);
        println("\tmove %s to *(rbp - %d)", argreg32[r], offset);
        return;
    case 8:
        // println("\tmov %s, %d(%%rbp)", argreg64[r], offset);
        println("\tmove %s to *(rbp - %d)", argreg64[r], offset);
        return;
  }
  Error("something is wrong");
}

static void EmitData(Obj* gvar) {
    // comment("emit data");
    for (Obj *var = gvar; var; var = var->next) {
        if (var->is_func) continue;

        
        
        if (var->init_data) {
            println("@.data");
            println("\tglobalize %s", var->name);
            print("\tdefine %s as [", var->name);
            print("%d", var->init_data[0]);
            for (int i = 1; i < var->type->array_len; i++)
                print(",%d", var->init_data[i]);
            println("] by 8bit");
        } else {
            println("@.bss");
            println("\tglobalize %s", var->name);
            println("\tallocate %s by 8bit for %d", var->name, var->type->size);
        }
    }
}

static void EmitFunc(Obj *func) {
    // comment("emit func");
    for (Obj *fn = func; fn; fn = fn->next) {
        if (!fn->is_func || !fn->is_def) continue;
        assert(fn->is_func);
        InitLVarOffset(fn);
        current_fn = fn;
        println("@.text");
        println("\tglobalize %s", fn->name);
        
        println("#%s", fn->name);

        println("\tpush rbp");
        println("\tmove rsp to rbp");
        println("\tsubstract %d from rsp", fn->stack_size);

        int i = 0;
        for (Obj *var = fn->params; var; var = var->next) {
            store_param(i++, var->offset, var->type->size);
        }

        for (Node *n = fn->body; n; n = n->next) {
            gen_stmt(n);
            assert(depth == 0);
        }
        println("#.L.return.%s", fn->name);
        println("\tmove rbp to rsp");
        println("\tpop rbp");
        println("\treturn");
    }
}

void GenCode(Obj *prog, FILE *out) {
    output_file = out;

    EmitData(prog);
    EmitFunc(prog);
}