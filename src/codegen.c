#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "5cc.h"

static int depth;

static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static Obj *current_fn;
static FILE *output_file;

static int indent;

static void put_newline() {
    fprintf(output_file, "\n");
    return;
}

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


static void print_indent() {
    for (int i = 0; i < indent; i++)
        print("    ");
}

static void print_comma() {
    print(", ");
}

static void enter_block() {
    print_indent();
    fprintf(output_file, "{\n");
    indent++;
}

static void leave_block() {
    indent--;
    print_indent();
    fprintf(output_file, "}\n");
}

static void print_label(char *label, bool is_global, char *section, bool does_enter_block, ...) {
    print_indent();
    print("<");
    va_list ap;
    va_start(ap, label);    
    vfprintf(output_file, label, ap);
    va_end(ap);
    if (is_global)
        print(" :global");
    if (strlen(section) != 0)
        print(" :%s", section);
    print(">");
    if (does_enter_block) {
        print(" {");
        indent++;
    }
    put_newline();
}

static void comment(char *msg) {
    print_indent();
    println("#%s", msg);
}

static void push(void) {
    print_indent();
    print("push(rax)");
    depth++;
}

static void pop(char *arg) {
    print("pop(%s)", arg);
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
    print_indent();
    if (type->size == 1)
        print("movsx(rax, ptr<byte>(rax, _, _, _))");
    else if (type->size == 2)
        print("movsx(rax, ptr<word>(rax, _, _, _))");
    else if (type->size == 4)
        print("movsx(rax, ptr<dword>(rax, _, _, _))");
    else
        print("mov(rax, ptr<qword>(rax, _, _, _))");
    put_newline();
    return;
}

static void store(Type *type) {
    print_indent();
    pop("rdi");
    if (type->kind == TY_STRUCT || type->kind == TY_UNION) {
        for (int i = 0; i < type->size; i++) {
            print_comma();
            print("mov(r8b, ptr(rax, _, _, %d))", i);
            print_comma();
            print("mov(ptr(rdi, _, _, %d), r8b)", i);
        }
        put_newline();
        return;
    }
    print_comma();
    if (type->size == 1)
        print("mov(ptr(rdi, _, _, _), al)");
    else if (type->size == 2)
        print("mov(ptr(rdi, _, _, _), ax)");
    else if (type->size == 4)
        print("mov(ptr(rdi, _, _, _), eax)");
    else
        print("mov(ptr(rdi, _, _, _), rax)");
    put_newline();
}

static void gen_stmt(Node *node);
static void gen_expr(Node *node);

static void gen_addr(Node *node) {
    switch (node->kind) {
    case ND_VAR:
        print_indent();
        if (node->var->is_lvar) {
            print("lea(rax, ptr(rbp, _, _, %d))", node->var->offset);
        } else {
            // print("\tlea %s(%%rip), %%rax", node->var->name);
            print("mov(rax, %s)", node->var->name);
        }
        put_newline();
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    case ND_DOTS:
        gen_addr(node->lhs);
        print_indent();
        print("add(rax, %d)", node->member->offset);
        put_newline();
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_addr(node->rhs);
        return;
    }
    Error("not an lvalue");
}

static void gen_expr(Node *node) {
    enter_block();
    switch (node->kind) {
    case ND_NUM:
        print_indent();
        print("mov(rax, %ld)", node->val);
        put_newline();
        goto end_return;
    case ND_NEG:
        gen_expr(node->lhs);
        print_indent();
        print("neg(rax)");
        put_newline();
        goto end_return;
    case ND_VAR:
        gen_addr(node);
        load(node->type);
        goto end_return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        put_newline();
        gen_expr(node->rhs);
        store(node->type);
        goto end_return;
    case ND_ADDR:
        gen_addr(node->lhs);
        goto end_return;
    case ND_DEREF:
        gen_expr(node->lhs);
        load(node->type);
        goto end_return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_expr(node->rhs);
        goto end_return;
    case ND_DOTS:
        gen_addr(node);
        load(node->type);
        goto end_return;
    case ND_STMT_EXPR:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
        goto end_return;
    case ND_FNCALL:{
        int nargs = 0;
        comment("function-call");
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            push();
            put_newline();
            nargs++;
        }

        if (nargs > 0) {
            print_indent();
            pop(argreg64[nargs - 1]);
            for (int i = nargs - 2; i >= 0; i--) {
                print_comma();
                pop(argreg64[i]);
            }
            put_newline();
        }
        
        print_indent();
        print("mov(rax, 0)");
        print_comma();
        print("call(%s)", node->fn_name);
        put_newline();
        goto end_return;
    }
    }
    gen_expr(node->rhs);
    push();
    put_newline();
    gen_expr(node->lhs);
    print_indent();
    pop("rdi");
    put_newline();

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
        print_indent();
        print("add(%s, %s)", ax, di);
        put_newline();
        goto end_return;
    case ND_SUB:
        print_indent();
        print("sub(%s, %s)", ax, di);
        put_newline();
        goto end_return;
    case ND_MUL:
        print_indent();
        print("imul(%s, %s)", ax, di);
        put_newline();
        goto end_return;
    case ND_DIV:
        print_indent();
        if (node->lhs->type->size == 8)
            print("cqo(), idiv(%s)", di);
        else
            print("cdq(), idiv(%s)", di);
        put_newline();
        goto end_return;
    case ND_EQ:
    case ND_NE:
    case ND_LE:
    case ND_LT:
        print_indent();
        if (node->kind == ND_EQ) {
            print("cmp(%s, %s), sete(al), movsx(rax, al)", ax, di);
        } else if (node->kind == ND_NE) {
            print("cmp(%s, %s), setne(al), movsx(rax, al)", ax, di);
        } else if (node->kind == ND_LT) {
            print("cmp(%s, %s), setl(al), movsx(rax, al)", ax, di);
        } else if (node->kind == ND_LE) {
            print("cmp(%s, %s), setle(al), movsx(rax, al)", ax, di);
        }
        put_newline();
        goto end_return;
    }

    Error("invalid expression");
end_return:
    leave_block();
    return;
}

static void gen_stmt(Node *node) {
    switch (node->kind) {
    case ND_EXPR_STMT:{
        // enter_block();
        gen_expr(node->lhs);
        // leave_block();
        return;
    }
    case ND_RETURN:{
        enter_block();
        gen_expr(node->lhs);
        print_indent();
        print("jmp(.L_return_%s)", current_fn->name);
        put_newline();
        leave_block();
        return;
    }
    case ND_BLOCK:{
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
        return;
    }
    case ND_IF:{
        int c = count(); 
        comment("if");
        enter_block();
        gen_expr(node->cond);
        print_indent();
        print("cmp(rax, 0), je(.L_else_%d)", c);
        put_newline();
        // print("je(L.else.%d)", c);
        gen_stmt(node->then);
        print_indent();
        print("jmp(.L_end_%d)", c);
        put_newline();
        // println(".L.else.%d:", c);
        print_label(".L_else_%d", false, "", node->_else, c);
        if (node->_else) {
            gen_stmt(node->_else);
            leave_block();
        }
        // println(".L.end.%d:", c);
        print_label(".L_end_%d", false, "", false, c);
        leave_block();
        return;
    }
    case ND_FOR:{
        int c = count();
        comment("for");
        enter_block();
        if (node->init) {
            comment("initialize");
            gen_stmt(node->init);
        }
        // println(".L.begin.%d:", c);
        
        print_label(".L_begin_%d", false, "", true, c);
        put_newline();
        if (node->cond) {
            comment("condition");
            gen_expr(node->cond);
            print_indent();
            print("cmp(rax, 0), je(.L_end_%d)", c);
            put_newline();
            // print("je(L.end.%d)", c);
        }
        comment("loop");
        gen_stmt(node->then);
        if (node->inc) {
            comment("inc");
            gen_expr(node->inc);
        }
        print_indent();
        print("jmp(.L_begin_%d)", c);
        put_newline();
        leave_block();
        // println(".L.end.%d:", c);
        print_label(".L_end_%d", false, "", false, c);
        leave_block();
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
    print_indent();
    switch (size) {
    case 1:
        print("mov(ptr(rbp, _, _, %d), %s)", offset, argreg8[r]);
        put_newline();
        return;
    case 2:
        print("mov(ptr(rbp, _, _, %d), %s)", offset, argreg16[r]);
        put_newline();
        return;
    case 4:
        print("mov(ptr(rbp, _, _, %d), %s)", offset, argreg32[r]);
        put_newline();
        return;
    case 8:
        print("mov(ptr(rbp, _, _, %d), %s)", offset, argreg64[r]);
        // println("\tmov %s, %d(%%rbp)", argreg64[r], offset);
        put_newline();
        return;
  }
  Error("something is wrong");
}

static void EmitData(Obj* gvar) {
    for (Obj *var = gvar; var; var = var->next) {
        if (var->is_func) continue;

        // println(".data");
        // println("\t.global %s", var->name);
        // println("%s:", var->name);
        // print_label(var->name, true, ".data", true);
        
        if (var->init_data) {
            print_label(var->name, true, ".data", true);
            print_indent();
            print("db(%d", var->init_data[0]);
            for (int i = 1; i < var->type->array_len; i++)
                // println("\t.byte %d", var->init_data[i]);
                print(", %d", var->init_data[i]);
            println(")");
        } else {
            print_label(var->name, true, ".bss", true);
            print_indent();
            print("resb(%d)", var->type->size);
            put_newline();
        }
        leave_block();
    }
}

static void EmitFunc(Obj *func) {
    for (Obj *fn = func; fn; fn = fn->next) {
        if (!fn->is_func || !fn->is_def) continue;
        assert(fn->is_func);
        InitLVarOffset(fn);
        current_fn = fn;
        // println(".text");
        // println("\t.globl %s", fn->name);
        // println("%s:", fn->name);

        print_label(fn->name, true, ".text", true);
        enter_block();
        print_indent();
        print("push(rbp)");
        print_comma();
        print("mov(rbp, rsp)");
        print_comma();
        print("sub(rsp, %d)", fn->stack_size);
        put_newline();
        leave_block();

        int i = 0;
        enter_block();
        for (Obj *var = fn->params; var; var = var->next) {
            store_param(i++, var->offset, var->type->size);
        }
        leave_block();

        for (Node *n = fn->body; n; n = n->next) {
            gen_stmt(n);
            assert(depth == 0);
        }
        // println(".L.return.%s:", fn->name);
        print_label(".L_return_%s", false, "", true, fn->name);
        print_indent();
        print("mov(rsp, rbp)");
        print_comma();
        print("pop(rbp)");
        print_comma();
        print("ret()");
        put_newline();
        leave_block();

        leave_block();
    }
}

void GenCode(Obj *prog, FILE *out) {
    output_file = out;
    print("extern(printf, assert_)");
    put_newline();
    EmitData(prog);
    EmitFunc(prog);
}