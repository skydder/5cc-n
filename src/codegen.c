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
        fprintf(output_file, "    ");
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

static void print_ins(char *fmt, ...) {
    print_indent();
    va_list ap;
    va_start(ap, fmt);    
    vfprintf(output_file, fmt, ap);
    va_end(ap);
    fprintf(output_file, "\n");
}

// static void println_ins(char *fmt, ...) {
//     print_indent();
//     va_list ap;
//     va_start(ap, fmt);    
//     vfprintf(output_file, fmt, ap);
//     va_end(ap);
//     fprintf(output_file, "\n");
// }

static void print_label(char *label, bool is_global, char *section, bool does_enter_block, ...) {
    print_indent();
    fprintf(output_file, "<");
    va_list ap;
    va_start(ap, label);    
    vfprintf(output_file, label, ap);
    va_end(ap);
    if (is_global)
        fprintf(output_file, " :global");
    if (strlen(section) != 0)
        fprintf(output_file, " :%s", section);
    fprintf(output_file, ">");
    if (does_enter_block) {
        fprintf(output_file, " {");
        indent++;
    }
    fprintf(output_file, "\n");
}

static void comment(char *msg) {
    print_indent();
    println("#%s", msg);
}

static void push(void) {
  print_ins("push(rax)");
  depth++;
}

static void pop(char *arg) {
  print_ins("pop(%s)", arg);
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
        print_ins("mov(rax, ptr<byte>(rax, _, _, _))");
    else if (type->size == 2)
        print_ins("mov(rax, ptr<word>(rax, _, _, _))");
    else if (type->size == 4)
        print_ins("mov(rax, ptr<dword>(rax, _, _, _))");
    else
        print_ins("mov(rax, ptr<qword>(rax, _, _, _))");
    return;
}

static void store(Type *type) {
    pop("%rdi");
    if (type->kind == TY_STRUCT || type->kind == TY_UNION) {
        for (int i = 0; i < type->size; i++) {
            print_ins("mov(r8b, ptr(rax, _, _, %d))", i);
            print_ins("mov(ptr(rdi, _, _, %d), r8b)", i);
        }
        return;
    }
    if (type->size == 1)
        print_ins("mov(ptr(rdi, _, _, _), al)");
    else if (type->size == 2)
        print_ins("mov(ptr(rdi, _, _, _), ax)");
    else if (type->size == 4)
        print_ins("mov(ptr(rdi, _, _, _), eax)");
    else
        print_ins("mov(ptr(rdi, _, _, _), rax)");
}

static void gen_stmt(Node *node);
static void gen_expr(Node *node);

static void gen_addr(Node *node) {
    switch (node->kind) {
    case ND_VAR:
        if (node->var->is_lvar) {
            print_ins("lea(rax, ptr(rbp, _, _, %d))", node->var->offset);
        } else {
            // print_ins("\tlea %s(%%rip), %%rax", node->var->name);
            print_ins("mov(rax, %s)", node->var->name);
        }
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    case ND_DOTS:
        gen_addr(node->lhs);
        print_ins("add(rax, %d)", node->member->offset);
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
        print_ins("mov(rax, %ld)", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        print_ins("neg(rax)");
        return;
    case ND_VAR:
        enter_block();
        gen_addr(node);
        load(node->type);
        leave_block();
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
        comment("function-call");
        enter_block();
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            push();
            nargs++;
        }
        
        for (int i = nargs - 1; i >= 0; i--)
            pop(argreg64[i]);

        print_ins("mov(rax, 0)");
        print_ins("call(%s)", node->fn_name);
        leave_block();
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
        print_ins("add(%s, %s)", ax, di);
        return;
    case ND_SUB:
        print_ins("sub(%s, %s)", ax, di);
        return;
    case ND_MUL:
        print_ins("imul(%s, %s)", ax, di);
        return;
    case ND_DIV:
        // if (node->lhs->type->size == 8)
        //     print_ins("cqo()");
        // else
        //     print_ins("cdq()");
        // print_ins("idiv(%s)", di);   
        if (node->lhs->type->size == 8)
            print_ins("cqo(), idiv(%s)", di);
        else
            print_ins("cdq(), idiv(%s)", di);
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LE:
    case ND_LT:
        // print_ins("cmp(%s, %s)", ax, di);
        // if (node->kind == ND_EQ) {
        //     print_ins("sete(al)");
        // } else if (node->kind == ND_NE) {
        //     print_ins("setne(al)");
        // } else if (node->kind == ND_LT) {
        //     print_ins("setl(al)");
        // } else if (node->kind == ND_LE) {
        //     print_ins("setle(al)");
        // }
        // print_ins("movzb(rax, al)");
        if (node->kind == ND_EQ) {
            print_ins("cmp(%s, %s), sete(al), movzx(rax, al)", ax, di);
        } else if (node->kind == ND_NE) {
            print_ins("cmp(%s, %s), setne(al), movzx(rax, al)", ax, di);
        } else if (node->kind == ND_LT) {
            print_ins("cmp(%s, %s), setl(al), movzx(rax, al)", ax, di);
        } else if (node->kind == ND_LE) {
            print_ins("cmp(%s, %s), setle(al), movzx(rax, al)", ax, di);
        }
        return;
    }

    Error("invalid expression");
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
        print_ins("jmp(.L_return_%s)", current_fn->name);
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
        enter_block();
        gen_expr(node->cond);
        print_ins("cmp(rax, 0), je(.L_else_%d)", c);
        // print_ins("je(L.else.%d)", c);
        gen_stmt(node->then);
        print_ins("jmp(.L_end_%d)", c);
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
        enter_block();
        if (node->init)
            gen_stmt(node->init);
        // println(".L.begin.%d:", c);
        print_label(".L_begin_%d", false, "", true, c);
        if (node->cond) {
            gen_expr(node->cond);
            print_ins("cmp(rax, 0), je(.L_end_%d)", c);
            // print_ins("je(L.end.%d)", c);
        }
        
        gen_stmt(node->then);
        if (node->inc)
            gen_expr(node->inc);
        print_ins("jmp(.L_begin_%d)", c);
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
    switch (size) {
    case 1:
        print_ins("mov(ptr(rbp, _, _, %d), %s)", offset, argreg8[r]);
        return;
    case 2:
        print_ins("mov(ptr(rbp, _, _, %d), %s)", offset, argreg16[r]);
        return;
    case 4:
        print_ins("mov(ptr(rbp, _, _, %d), %s)", offset, argreg32[r]);
        return;
    case 8:
        print_ins("mov(ptr(rbp, _, _, %d), %s)", offset, argreg64[r]);
        // println("\tmov %s, %d(%%rbp)", argreg64[r], offset);
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
                print(", %d", var->init_data[0]);
            println(")");
        } else {
            print_label(var->name, true, ".bss", true);
            print_ins("resb(%d)", var->type->size);
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
        print_ins("push(rbp)");
        print_ins("mov(rbp, rsp)");
        print_ins("sub(rsp, %d)", fn->stack_size);
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
        print_ins("mov(rsp, rbp)");
        print_ins("pop(rbp)");
        print_ins("ret()");
        leave_block();

        leave_block();
    }
}

void GenCode(Obj *prog, FILE *out) {
    output_file = out;
    print_ins("extern(printf, assert_)");
    EmitData(prog);
    EmitFunc(prog);
}