/* sema.c - semantic analysis: name resolution, typing, OOP layout.
 * Produces a resolved/annotated AST that the borrow checker and the
 * code generator work on. Identifiers are case-sensitive. */
#include "ast.h"
#include <limits.h>

typedef enum { SY_VAR, SY_CONST, SY_TYPE, SY_ROUTINE } SymKind;
typedef struct {
    SymKind kind;
    char *name;
    Var *var;
    Type *type;
    Expr *cval;
    Routine *rt;
} Sym;

typedef struct Scope {
    VEC(Sym *) syms;
    Var *with;
    struct Scope *up;
} Scope;

enum { LK_NONE, LK_SYM, LK_MEMBER };
typedef struct {
    int kind;
    Sym *sym;
    Var *base;
    Field *f;
    Routine *m;
    Prop *p;
} Look;

static Program *prog;
static Scope *scope;
static Routine *cur;
static int level;
static int loop_depth;
static int in_prelude;
static int var_counter, routine_counter, struct_counter, enum_counter, with_counter;
static StmtVec *local_init;

static Type *t_void, *t_int, *t_i64, *t_i8, *t_u8, *t_i16, *t_u16, *t_u32, *t_bool, *t_char,
    *t_real, *t_str, *t_ptr, *t_nil;

static Expr *value(Expr *e);
static Expr *ex(Expr *e);
static Type *resolve_type(TypeExpr *te);
static void decls(Block *b, int has_frame);
static void stmts(StmtVec *v);

/* ============================================================== types */

static Type *new_type(TypeKind k, const char *name) {
    Type *t = NEW(Type);
    t->kind = k;
    t->name = name ? xstrdup(name) : NULL;
    return t;
}
static Type *int_type(int ik, const char *name) {
    Type *t = new_type(T_INT, name);
    t->ik = ik;
    return t;
}
static Type *ptr_to(Type *b) {
    Type *t = new_type(T_PTR, NULL);
    t->base = b;
    return t;
}

const char *type_name(Type *t) {
    if (!t) return "?";
    if (t->name) return t->name;
    switch (t->kind) {
    case T_VOID: return "untyped";
    case T_INT: return "Integer";
    case T_BOOL: return "Boolean";
    case T_CHAR: return "Char";
    case T_REAL: return "Real";
    case T_STR: return "String";
    case T_NIL: return "Pointer(nil)";
    case T_PTR: return t->base ? sfmt("^%s", type_name(t->base)) : "Pointer";
    case T_ARRAY: return sfmt("array[%lld..%lld] of %s", t->lo, t->hi, type_name(t->base));
    case T_RECORD: case T_OBJECT: case T_CLASS: return t->st->name;
    case T_ENUM: return t->en->name;
    }
    return "?";
}

static int is_int(Type *t) { return t->kind == T_INT; }
static int is_ordinal(Type *t) {
    return t->kind == T_INT || t->kind == T_CHAR || t->kind == T_BOOL || t->kind == T_ENUM;
}
static int is_numeric(Type *t) { return t->kind == T_INT || t->kind == T_REAL; }
static int is_strlike(Type *t) { return t->kind == T_STR || t->kind == T_CHAR; }
static int is_ptrlike(Type *t) { return t->kind == T_PTR || t->kind == T_CLASS || t->kind == T_NIL; }

/* owning types: class instances and typed pointers (borrow checker) */
int is_owning_type(Type *t) { return t && (t->kind == T_CLASS || (t->kind == T_PTR && t->base)); }

int struct_descends(Struct *d, Struct *b) {
    for (; d; d = d->parent)
        if (d == b) return 1;
    return 0;
}

static int same_type(Type *a, Type *b) {
    if (a == b) return 1;
    if (a->kind != b->kind) return 0;
    switch (a->kind) {
    case T_INT: return a->ik == b->ik;
    case T_PTR:
        if (!a->base || !b->base) return !a->base && !b->base;
        return same_type(a->base, b->base);
    case T_ARRAY: return a->lo == b->lo && a->hi == b->hi && same_type(a->base, b->base);
    case T_RECORD: case T_OBJECT: case T_CLASS: return a->st == b->st;
    case T_ENUM: return a->en == b->en;
    default: return 1;
    }
}

static Type *arith_int(Type *a, Type *b) {
    if ((a->kind == T_INT && (a->ik == IK_I64 || a->ik == IK_U32)) ||
        (b->kind == T_INT && (b->ik == IK_I64 || b->ik == IK_U32)))
        return t_i64;
    return t_int;
}

/* 0 = incompatible, 1 = compatible as is, 2 = needs conversion node */
static int assign_compat(Type *to, Type *from) {
    if (same_type(to, from)) return 1;
    switch (to->kind) {
    case T_INT: return from->kind == T_INT;
    case T_REAL: return from->kind == T_INT ? 2 : 0;
    case T_STR: return from->kind == T_CHAR ? 2 : 0;
    case T_PTR:
        if (from->kind == T_NIL) return 1;
        if (from->kind == T_PTR) {
            if (!to->base || !from->base) return 2;
            if (to->base->st && from->base->st && to->base->kind == T_OBJECT &&
                from->base->kind == T_OBJECT && struct_descends(from->base->st, to->base->st))
                return 2;
        }
        return 0;
    case T_CLASS:
        if (from->kind == T_NIL) return 1;
        if (from->kind == T_CLASS && struct_descends(from->st, to->st)) return 2;
        return 0;
    case T_OBJECT:
        if (from->kind == T_OBJECT && struct_descends(from->st, to->st)) return 2;
        return 0;
    default: return 0;
    }
}

static Expr *conv(Expr *e, Type *to) {
    Expr *c = new_expr(E_CONV, e->pos);
    c->a = e;
    c->type = to;
    return c;
}

static Expr *coerce(Expr *e, Type *to, const char *ctx) {
    int c = assign_compat(to, e->type);
    if (!c)
        error_at(e->pos, "incompatible types in %s: got \"%s\" expected \"%s\"", ctx, type_name(e->type),
                 type_name(to));
    if (c == 2) return conv(e, to);
    return e;
}

/* ============================================================== scopes */

static Scope *push_scope(void) {
    Scope *s = NEW(Scope);
    s->up = scope;
    scope = s;
    return s;
}
static void pop_scope(void) { scope = scope->up; }

static Sym *find_local(const char *name) {
    for (int i = 0; i < scope->syms.len; i++)
        if (STREQ(scope->syms.items[i]->name, name)) return scope->syms.items[i];
    return NULL;
}

static Sym *add_sym(SymKind k, const char *name, Pos p) {
    if (find_local(name)) error_at(p, "duplicate identifier \"%s\"", name);
    Sym *s = NEW(Sym);
    s->kind = k;
    s->name = xstrdup(name);
    vpush(scope->syms, s);
    return s;
}

static int find_member(Struct *st, const char *name, Field **f, Routine **m, Prop **p) {
    *f = NULL;
    *m = NULL;
    *p = NULL;
    for (int i = 0; i < st->fields.len; i++)
        if (STREQ(st->fields.items[i]->name, name)) {
            *f = st->fields.items[i];
            return 1;
        }
    for (Struct *s = st; s; s = s->parent) {
        for (int i = 0; i < s->methods.len; i++)
            if (STREQ(s->methods.items[i]->name, name)) {
                *m = s->methods.items[i];
                return 1;
            }
        for (int i = 0; i < s->props.len; i++)
            if (STREQ(s->props.items[i]->name, name)) {
                *p = s->props.items[i];
                return 1;
            }
    }
    return 0;
}

static Look lookup(const char *name) {
    Look L = {0};
    for (Scope *s = scope; s; s = s->up) {
        if (s->with) {
            Field *f;
            Routine *m;
            Prop *p;
            if (find_member(s->with->type->st, name, &f, &m, &p)) {
                L.kind = LK_MEMBER;
                L.base = s->with;
                L.f = f;
                L.m = m;
                L.p = p;
                return L;
            }
        }
        for (int i = 0; i < s->syms.len; i++)
            if (STREQ(s->syms.items[i]->name, name)) {
                L.kind = LK_SYM;
                L.sym = s->syms.items[i];
                return L;
            }
    }
    return L;
}

/* ============================================================== vars */

static Var *new_var(const char *name, Type *t, int vkind, Pos p) {
    Var *v = NEW(Var);
    v->name = xstrdup(name);
    v->type = t;
    v->vkind = vkind;
    v->level = level;
    v->owner = cur;
    v->id = ++var_counter;
    v->pos = p;
    v->is_global = (level == 0 && vkind == VK_GLOBAL);
    switch (vkind) {
    case VK_GLOBAL: v->cname = sfmt("G_%s", name); break;
    case VK_PARAM: v->cname = sfmt("p_%s", name); break;
    case VK_SELF: v->cname = xstrdup("self"); break;
    case VK_RESULT: v->cname = xstrdup("Result"); break;
    case VK_WITH: v->cname = sfmt("__w%d", ++with_counter); break;
    default: v->cname = sfmt("L_%s", name); break;
    }
    return v;
}

static Expr *var_expr(Var *v, Pos p) {
    Expr *e = new_expr(E_VAR, p);
    e->var = v;
    e->type = v->type;
    return e;
}

static int is_lvalue(Expr *e) {
    switch (e->kind) {
    case E_VAR: return 1;
    case E_FIELD: return e->a->type->kind == T_CLASS || is_lvalue(e->a);
    case E_INDEX: return is_lvalue(e->a);
    case E_DEREF: return 1;
    case E_CAST: return (e->type->kind == T_RECORD || e->type->kind == T_OBJECT) && is_lvalue(e->a);
    default: return 0;
    }
}

static void check_writable(Expr *e, const char *what) {
    if (!is_lvalue(e)) error_at(e->pos, "%s: variable identifier expected", what);
    if (e->kind == E_VAR && e->var->is_const)
        error_at(e->pos, "%s: cannot modify constant/loop variable \"%s\"", what, e->var->name);
}

/* ============================================================== constants */

static Expr *lit_int(long long v, Type *t, Pos p) {
    Expr *e = new_expr(E_INT, p);
    e->ival = v;
    e->type = t;
    return e;
}

static int lit_ord(Expr *e, long long *v) {
    if (e->kind == E_INT) {
        *v = e->ival;
        return 1;
    }
    if (e->kind == E_STR && e->type->kind == T_CHAR) {
        *v = (unsigned char)e->sval[0];
        return 1;
    }
    return 0;
}

/* fold a resolved expression into a literal; NULL if not constant */
static Expr *fold(Expr *e) {
    long long x, y;
    switch (e->kind) {
    case E_INT: case E_REAL: case E_STR: return e;
    case E_UNARY: {
        Expr *a = fold(e->a);
        if (!a) return NULL;
        if (a->kind == E_REAL) {
            Expr *r = new_expr(E_REAL, e->pos);
            r->type = t_real;
            r->rval = e->op == OP_NEG ? -a->rval : a->rval;
            return e->op == OP_NOT ? NULL : r;
        }
        if (!lit_ord(a, &x)) return NULL;
        if (e->op == OP_NEG) return lit_int(-x, e->type, e->pos);
        if (e->op == OP_PLUS) return lit_int(x, e->type, e->pos);
        if (e->type->kind == T_BOOL) return lit_int(!x, t_bool, e->pos);
        return lit_int(~x, e->type, e->pos);
    }
    case E_CONV: {
        Expr *a = fold(e->a);
        if (!a) return NULL;
        if (e->type->kind == T_REAL && a->kind == E_INT) {
            Expr *r = new_expr(E_REAL, e->pos);
            r->type = t_real;
            r->rval = (double)a->ival;
            return r;
        }
        if (e->type->kind == T_STR && lit_ord(a, &x)) {
            Expr *r = new_expr(E_STR, e->pos);
            r->type = t_str;
            char c = (char)x;
            r->sval = xstrndup(&c, 1);
            r->slen = 1;
            return r;
        }
        return NULL;
    }
    case E_CAST: {
        Expr *a = fold(e->a);
        if (a && is_ordinal(e->type) && lit_ord(a, &x)) return lit_int(x, e->type, e->pos);
        return NULL;
    }
    case E_BUILTIN: {
        if (e->args.len != 1) return NULL;
        Expr *a = fold(e->args.items[0]);
        if (!a) return NULL;
        if ((e->bi == BI_ORD || e->bi == BI_CHR || e->bi == BI_SUCC || e->bi == BI_PRED) && lit_ord(a, &x)) {
            if (e->bi == BI_SUCC) x++;
            if (e->bi == BI_PRED) x--;
            return lit_int(x, e->type, e->pos);
        }
        if (e->bi == BI_LENGTH && a->kind == E_STR) return lit_int(a->slen, t_int, e->pos);
        return NULL;
    }
    case E_BINARY: {
        Expr *a = fold(e->a), *b = fold(e->b);
        if (!a || !b) return NULL;
        if (a->kind == E_STR && b->kind == E_STR && a->type->kind == T_STR) {
            if (e->op == OP_ADD) {
                Expr *r = new_expr(E_STR, e->pos);
                r->type = t_str;
                r->slen = a->slen + b->slen;
                r->sval = xmalloc((size_t)r->slen + 1);
                memcpy(r->sval, a->sval, (size_t)a->slen);
                memcpy(r->sval + a->slen, b->sval, (size_t)b->slen);
                return r;
            }
            return NULL;
        }
        if (a->kind == E_REAL || b->kind == E_REAL) {
            double p = a->kind == E_REAL ? a->rval : (double)a->ival;
            double q = b->kind == E_REAL ? b->rval : (double)b->ival;
            double r;
            switch (e->op) {
            case OP_ADD: r = p + q; break;
            case OP_SUB: r = p - q; break;
            case OP_MUL: r = p * q; break;
            case OP_DIV: r = p / q; break;
            case OP_EQ: return lit_int(p == q, t_bool, e->pos);
            case OP_NE: return lit_int(p != q, t_bool, e->pos);
            case OP_LT: return lit_int(p < q, t_bool, e->pos);
            case OP_LE: return lit_int(p <= q, t_bool, e->pos);
            case OP_GT: return lit_int(p > q, t_bool, e->pos);
            case OP_GE: return lit_int(p >= q, t_bool, e->pos);
            default: return NULL;
            }
            Expr *re = new_expr(E_REAL, e->pos);
            re->type = t_real;
            re->rval = r;
            return re;
        }
        if (!lit_ord(a, &x) || !lit_ord(b, &y)) return NULL;
        switch (e->op) {
        case OP_ADD: return lit_int(x + y, e->type, e->pos);
        case OP_SUB: return lit_int(x - y, e->type, e->pos);
        case OP_MUL: return lit_int(x * y, e->type, e->pos);
        case OP_IDIV:
            if (!y) error_at(e->pos, "division by zero");
            return lit_int(x / y, e->type, e->pos);
        case OP_MOD:
            if (!y) error_at(e->pos, "division by zero");
            return lit_int(x % y, e->type, e->pos);
        case OP_AND: return lit_int(e->type->kind == T_BOOL ? (x && y) : (x & y), e->type, e->pos);
        case OP_OR: return lit_int(e->type->kind == T_BOOL ? (x || y) : (x | y), e->type, e->pos);
        case OP_XOR: return lit_int(e->type->kind == T_BOOL ? (!x != !y) : (x ^ y), e->type, e->pos);
        case OP_SHL: return lit_int(x << y, e->type, e->pos);
        case OP_SHR: return lit_int((long long)((unsigned long long)x >> y), e->type, e->pos);
        case OP_EQ: return lit_int(x == y, t_bool, e->pos);
        case OP_NE: return lit_int(x != y, t_bool, e->pos);
        case OP_LT: return lit_int(x < y, t_bool, e->pos);
        case OP_LE: return lit_int(x <= y, t_bool, e->pos);
        case OP_GT: return lit_int(x > y, t_bool, e->pos);
        case OP_GE: return lit_int(x >= y, t_bool, e->pos);
        }
        return NULL;
    }
    default: return NULL;
    }
}

static Expr *const_expr(Expr *e) {
    Expr *v = value(e);
    Expr *f = fold(v);
    if (!f) error_at(e->pos, "constant expression expected");
    return f;
}

static long long const_ord(Expr *e, Type **ty) {
    Expr *f = const_expr(e);
    long long v;
    if (!lit_ord(f, &v)) error_at(e->pos, "ordinal constant expected");
    if (ty) *ty = f->type;
    return v;
}

/* ============================================================== type resolution */

static Struct *new_struct(const char *name, int kind, Pos p) {
    Struct *s = NEW(Struct);
    s->id = ++struct_counter;
    s->name = xstrdup(name);
    s->kind = kind;
    s->pos = p;
    s->type = new_type(kind == SK_RECORD ? T_RECORD : kind == SK_OBJECT ? T_OBJECT : T_CLASS, name);
    s->type->st = s;
    return s;
}

static void check_complete(Type *t, Pos p) {
    if ((t->kind == T_RECORD || t->kind == T_OBJECT) && !t->st->defined)
        error_at(p, "type \"%s\" is not completely defined", t->st->name);
}

static void add_fields(Struct *s, FieldDeclVec *fields) {
    for (int i = 0; i < fields->len; i++) {
        FieldDecl *fd = &fields->items[i];
        Type *t = resolve_type(fd->ty);
        check_complete(t, fd->ty->pos);
        if (fd->borrowed && !is_owning_type(t))
            error_at(fd->ty->pos, "'borrowed' requires a class or typed pointer type");
        for (int k = 0; k < fd->names.len; k++) {
            Ident *id = &fd->names.items[k];
            for (int j = 0; j < s->fields.len; j++)
                if (STREQ(s->fields.items[j]->name, id->name))
                    error_at(id->pos, "duplicate field \"%s\"", id->name);
            Field *f = NEW(Field);
            f->name = id->name;
            f->cname = sfmt("f_%s", id->name);
            f->type = t;
            f->borrowed = fd->borrowed;
            f->owner = s;
            vpush(s->fields, f);
        }
    }
}

static void define_record(Struct *s, FieldDeclVec *fields) {
    add_fields(s, fields);
    s->defined = 1;
    vpush(prog->structs, s);
}

static long long range_of(TypeExpr *te, long long *lo, Type **idx) {
    if (te->kind == TE_SUBRANGE) {
        Type *a, *b;
        *lo = const_ord(te->lo, &a);
        long long hi = const_ord(te->hi, &b);
        if (a->kind != b->kind) error_at(te->pos, "subrange bounds have different types");
        if (hi < *lo) error_at(te->pos, "invalid subrange %lld..%lld", *lo, hi);
        *idx = a->kind == T_INT ? t_int : a;
        return hi;
    }
    Type *t = resolve_type(te);
    *idx = t;
    switch (t->kind) {
    case T_CHAR: *lo = 0; return 255;
    case T_BOOL: *lo = 0; return 1;
    case T_ENUM: *lo = 0; return t->en->names.len - 1;
    case T_INT:
        if (t->ik == IK_U8) { *lo = 0; return 255; }
        if (t->ik == IK_I8) { *lo = -128; return 127; }
        break;
    default: break;
    }
    error_at(te->pos, "invalid array index type \"%s\"", type_name(t));
}

static Type *resolve_type(TypeExpr *te) {
    switch (te->kind) {
    case TE_NAMED: {
        Look L = lookup(te->name.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE)
            error_at(te->pos, "type identifier expected, \"%s\" is %s", te->name.name,
                     L.kind == LK_NONE ? "unknown" : "not a type");
        return L.sym->type;
    }
    case TE_PTR: {
        Look L = lookup(te->name.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE)
            error_at(te->pos, "type identifier expected after '^' (\"%s\")", te->name.name);
        return ptr_to(L.sym->type);
    }
    case TE_STR: return t_str;
    case TE_ARRAY: {
        Type *elem = resolve_type(te->elem);
        check_complete(elem, te->elem->pos);
        for (int i = te->ranges.len - 1; i >= 0; i--) {
            Type *t = new_type(T_ARRAY, NULL);
            t->hi = range_of(te->ranges.items[i], &t->lo, &t->index);
            if (t->hi - t->lo > 100000000) error_at(te->pos, "array too large");
            t->base = elem;
            elem = t;
        }
        return elem;
    }
    case TE_SUBRANGE: {
        Type *a;
        const_ord(te->lo, &a);
        const_ord(te->hi, NULL);
        return a->kind == T_INT ? t_int : a;
    }
    case TE_ENUM: {
        EnumInfo *en = NEW(EnumInfo);
        en->id = ++enum_counter;
        en->name = sfmt("enum%d", en->id);
        Type *t = new_type(T_ENUM, NULL);
        t->en = en;
        for (int i = 0; i < te->enums.len; i++) {
            vpush(en->names, te->enums.items[i].name);
            Sym *s = add_sym(SY_CONST, te->enums.items[i].name, te->enums.items[i].pos);
            s->cval = lit_int(i, t, te->enums.items[i].pos);
            s->type = t;
        }
        vpush(prog->enums, en);
        return t;
    }
    case TE_RECORD: {
        Struct *s = new_struct(sfmt("record%d", struct_counter + 1), SK_RECORD, te->pos);
        define_record(s, &te->fields);
        return s->type;
    }
    case TE_STRUCT:
    case TE_CLASSFWD: error_at(te->pos, "object and class types must be declared in a type section");
    }
    return t_void;
}

/* ============================================================== routines */

static void build_params(Routine *rt, ParamGroupVec *pg) {
    rt->params.len = 0;
    for (int i = 0; i < pg->len; i++) {
        ParamGroup *g = &pg->items[i];
        Type *t = resolve_type(g->ty);
        if (g->mode == PM_OWNED && !is_owning_type(t))
            error_at(g->ty->pos, "'owned' parameters require a class or typed pointer type");
        for (int k = 0; k < g->names.len; k++) {
            Param p = {g->names.items[k].name, t, g->mode, NULL};
            vpush(rt->params, p);
        }
    }
}

static int same_signature(Routine *a, Routine *b) {
    if (a->params.len != b->params.len || a->kind != b->kind) return 0;
    for (int i = 0; i < a->params.len; i++) {
        if (a->params.items[i].mode != b->params.items[i].mode) return 0;
        if (!same_type(a->params.items[i].type, b->params.items[i].type)) return 0;
    }
    if (a->ret || b->ret) {
        if (!a->ret || !b->ret || !same_type(a->ret, b->ret)) return 0;
    }
    return 1;
}

static Routine *new_routine(const char *name, int kind, Pos p) {
    Routine *r = NEW(Routine);
    r->id = ++routine_counter;
    r->name = xstrdup(name);
    r->kind = kind;
    r->slot = -1;
    r->pos = p;
    r->prelude = in_prelude;
    vpush(prog->routines, r);
    return r;
}

static int find_slot(Struct *s, const char *name) {
    for (int i = s->slots.len - 1; i >= 0; i--)
        if (STREQ(s->slots.items[i].name, name)) return i;
    return -1;
}

static void define_struct(Struct *s, StructDecl *sd) {
    Struct *par = NULL;
    if (sd->has_parent) {
        Look L = lookup(sd->parent.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE ||
            (L.sym->type->kind != T_OBJECT && L.sym->type->kind != T_CLASS))
            error_at(sd->parent.pos, "\"%s\" is not an object or class type", sd->parent.name);
        par = L.sym->type->st;
        if (par->kind != s->kind)
            error_at(sd->parent.pos, "cannot mix 'object' and 'class' in inheritance");
        if (!par->defined) error_at(sd->parent.pos, "parent type \"%s\" is not completely defined", par->name);
    } else if (s->kind == SK_CLASS && prog->tobject && s != prog->tobject)
        par = prog->tobject;
    s->parent = par;
    if (par) {
        for (int i = 0; i < par->fields.len; i++) vpush(s->fields, par->fields.items[i]);
        for (int i = 0; i < par->slots.len; i++) vpush(s->slots, par->slots.items[i]);
        s->has_vmt = par->has_vmt;
    }
    int any_virtual = 0;
    for (int i = 0; i < sd->methods.len; i++)
        if (sd->methods.items[i].virt != VIRT_NONE) any_virtual = 1;
    if (!s->has_vmt && any_virtual) {
        Field *f = NEW(Field);
        f->name = "__vmt";
        f->cname = "__vmt";
        f->type = t_ptr;
        f->owner = s;
        vpush(s->fields, f);
        s->has_vmt = 1;
    }
    add_fields(s, &sd->fields);

    for (int i = 0; i < sd->methods.len; i++) {
        MethodDecl *md = &sd->methods.items[i];
        for (int j = 0; j < s->methods.len; j++)
            if (STREQ(s->methods.items[j]->name, md->name.name))
                error_at(md->name.pos, "duplicate method \"%s\" (overloading is not supported)", md->name.name);
        Routine *r = new_routine(md->name.name, md->kind, md->name.pos);
        r->owner = s;
        r->level = 1;
        r->cname = sfmt("M%d_%s", s->id, md->name.name);
        build_params(r, &md->params);
        if (md->ret) r->ret = resolve_type(md->ret);
        r->ret_borrowed = md->ret_borrowed;
        if (md->ret_borrowed && !is_owning_type(r->ret))
            error_at(md->name.pos, "'borrowed' result requires a class or typed pointer type");
        r->virt = md->virt;
        r->is_abstract = md->is_abstract;
        if (md->kind == RK_CTOR && md->virt != VIRT_NONE)
            error_at(md->name.pos, "virtual constructors are not supported");
        if (md->is_abstract && md->virt == VIRT_NONE)
            error_at(md->name.pos, "abstract methods must be virtual");
        if (md->virt == VIRT_OVERRIDE ||
            (md->virt == VIRT_VIRTUAL && s->kind == SK_OBJECT && find_slot(s, md->name.name) >= 0)) {
            int idx = find_slot(s, md->name.name);
            if (idx < 0) error_at(md->name.pos, "there is no method in an ancestor class to be overridden: \"%s\"", md->name.name);
            if (!same_signature(s->slots.items[idx].intro, r))
                error_at(md->name.pos, "overriding method \"%s\" has a different signature", md->name.name);
            s->slots.items[idx].impl = r;
            r->slot = idx;
        } else if (md->virt == VIRT_VIRTUAL) {
            Slot sl = {r->name, r, r};
            vpush(s->slots, sl);
            r->slot = s->slots.len - 1;
        }
        if (r->is_abstract) r->defined = 1;
        vpush(s->methods, r);
    }
    for (int i = 0; i < sd->props.len; i++) {
        PropDecl *pd = &sd->props.items[i];
        Prop *p = NEW(Prop);
        p->name = pd->name.name;
        p->type = resolve_type(pd->ty);
        p->read = pd->has_read ? pd->read.name : NULL;
        p->write = pd->has_write ? pd->write.name : NULL;
        p->pos = pd->name.pos;
        vpush(s->props, p);
    }
    s->defined = 1;
    vpush(prog->structs, s);
}

/* ============================================================== calls */

static Routine *enclosing_method(void) {
    for (Routine *r = cur; r; r = r->parent)
        if (r->owner) return r;
    return NULL;
}

static int var_compat(Type *formal, Type *actual) {
    if (same_type(formal, actual)) return 1;
    if ((formal->kind == T_OBJECT || formal->kind == T_CLASS) && formal->kind == actual->kind &&
        struct_descends(actual->st, formal->st))
        return 1;
    return 0;
}

static Expr *make_call(Routine *rt, Expr *base, ExprVec *args, int mode, Type *newcls, Pos p) {
    ExprVec empty = {0};
    if (!args) args = &empty;
    if (args->len != rt->params.len)
        error_at(p, "wrong number of arguments calling \"%s\" (expected %d, got %d)", rt->name, rt->params.len,
                 args->len);
    Expr *c = new_expr(E_RCALL, p);
    c->rt = rt;
    c->a = base;
    for (int i = 0; i < args->len; i++) {
        Param *pm = &rt->params.items[i];
        Expr *a = args->items[i];
        if (pm->mode == PM_VAR || pm->mode == PM_OUT) {
            a = value(a);
            check_writable(a, sfmt("argument %d of \"%s\"", i + 1, rt->name));
            if (!var_compat(pm->type, a->type))
                error_at(a->pos, "call by var for arg no. %d has to match exactly: got \"%s\" expected \"%s\"", i + 1,
                         type_name(a->type), type_name(pm->type));
        } else
            a = coerce(value(a), pm->type, sfmt("argument %d of \"%s\"", i + 1, rt->name));
        vpush(c->args, a);
    }
    if (mode < 0) {
        if (rt->owner) {
            if (rt->kind == RK_CTOR) mode = newcls ? CM_NEWCLASS : CM_CTOR_INST;
            else mode = rt->slot >= 0 ? CM_VIRTUAL : CM_STATIC;
        } else
            mode = CM_NORMAL;
    }
    if (rt->owner && !base && mode != CM_NEWCLASS && mode != CM_CTOR_NEW)
        error_at(p, "method \"%s\" needs an instance", rt->name);
    if ((mode == CM_INHERITED || mode == CM_STATIC) && rt->is_abstract)
        error_at(p, "abstract method \"%s\" cannot be called directly", rt->name);
    c->cmode = mode;
    c->ctype = newcls;
    if (rt->kind == RK_FUNC) c->type = rt->ret;
    else if (rt->kind == RK_CTOR && rt->owner->kind == SK_CLASS) c->type = newcls ? newcls : base->type;
    else c->type = t_void;
    return c;
}

static Expr *autoderef(Expr *e) {
    if (e->type->kind == T_PTR && e->type->base &&
        (e->type->base->kind == T_RECORD || e->type->base->kind == T_OBJECT)) {
        Expr *d = new_expr(E_DEREF, e->pos);
        d->a = e;
        d->type = e->type->base;
        return d;
    }
    return e;
}

static Expr *member_ref(Expr *base, Look *L, ExprVec *args, Pos p, const char *name) {
    if (L->f) {
        if (args) error_at(p, "\"%s\" is a field, not a method", name);
        Expr *e = new_expr(E_FIELD, p);
        e->a = base;
        e->field = L->f;
        e->type = L->f->type;
        return e;
    }
    if (L->m) return make_call(L->m, base, args, -1, NULL, p);
    if (args) error_at(p, "\"%s\" is a property, not a method", name);
    Prop *pr = L->p;
    if (!pr->read) error_at(p, "property \"%s\" is write-only", name);
    Look R = {0};
    if (!find_member(base->type->st, pr->read, &R.f, &R.m, &R.p) || R.p)
        error_at(pr->pos, "property reader \"%s\" not found", pr->read);
    if (R.m && (R.m->kind != RK_FUNC || R.m->params.len))
        error_at(pr->pos, "property reader \"%s\" must be a parameterless function", pr->read);
    Expr *e = member_ref(base, &R, NULL, p, pr->read);
    if (!same_type(e->type, pr->type)) error_at(pr->pos, "property reader type mismatch");
    return e;
}

static Expr *cast_expr(Type *t, ExprVec *args, Pos p) {
    if (!args || args->len != 1) error_at(p, "type cast needs exactly one argument");
    Expr *a = value(args->items[0]);
    if (same_type(t, a->type)) return a;
    Type *f = a->type;
    int ok = 0;
    if ((is_ordinal(t)) && (is_ordinal(f))) ok = 1;
    else if (t->kind == T_REAL && is_numeric(f)) ok = 1;
    else if ((t->kind == T_CLASS || t->kind == T_PTR) && (is_ptrlike(f) || is_int(f))) ok = 1;
    else if (is_int(t) && (f->kind == T_PTR || f->kind == T_CLASS)) ok = 1;
    else if (t->kind == T_STR && is_strlike(f)) ok = 1;
    else if ((t->kind == T_OBJECT || t->kind == T_RECORD) && f->kind == t->kind && is_lvalue(a)) ok = 1;
    if (!ok) error_at(p, "illegal type conversion: \"%s\" to \"%s\"", type_name(f), type_name(t));
    if (t->kind == T_STR) return conv(a, t_str);
    Expr *e = new_expr(E_CAST, p);
    e->a = a;
    e->type = t;
    e->ctype = t;
    return e;
}

static Expr *builtin(int bi, ExprVec *args, Pos p);
static int builtin_id(const char *name);

static Expr *resolve_ref(Expr *e, ExprVec *args) {
    Pos p = e->pos;
    if (e->kind == E_IDENT) {
        const char *name = e->id.name;
        Look L = lookup(name);
        if (L.kind == LK_NONE) {
            int bi = builtin_id(name);
            if (bi) return builtin(bi, args, p);
            error_at(p, "identifier not found \"%s\"", name);
        }
        if (L.kind == LK_MEMBER) return member_ref(var_expr(L.base, p), &L, args, p, name);
        Sym *s = L.sym;
        switch (s->kind) {
        case SY_VAR:
            if (args) error_at(p, "\"%s\" is a variable and cannot be called", name);
            return var_expr(s->var, p);
        case SY_CONST: {
            if (args) error_at(p, "\"%s\" is a constant and cannot be called", name);
            Expr *c = NEW(Expr);
            *c = *s->cval;
            c->pos = p;
            return c;
        }
        case SY_TYPE:
            if (args) return cast_expr(s->type, args, p);
            {
                Expr *t = new_expr(E_TYPEREF, p);
                t->ctype = s->type;
                t->type = s->type;
                return t;
            }
        case SY_ROUTINE: return make_call(s->rt, NULL, args, -1, NULL, p);
        }
    }
    if (e->kind == E_MEMBER) {
        const char *name = e->id.name;
        if (e->a->kind == E_IDENT) {
            Look L = lookup(e->a->id.name);
            if (L.kind == LK_SYM && L.sym->kind == SY_TYPE) {
                Type *t = L.sym->type;
                if (t->kind == T_CLASS) {
                    Look M = {0};
                    if (!find_member(t->st, name, &M.f, &M.m, &M.p) || !M.m || M.m->kind != RK_CTOR)
                        error_at(p, "\"%s.%s\" is not a constructor", t->st->name, name);
                    return make_call(M.m, NULL, args, CM_NEWCLASS, t, p);
                }
                if (t->kind == T_ENUM) {
                    for (int i = 0; i < t->en->names.len; i++)
                        if (STREQ(t->en->names.items[i], name)) return lit_int(i, t, p);
                    error_at(p, "\"%s\" is not a value of the enumeration", name);
                }
                error_at(p, "illegal qualifier on type \"%s\"", type_name(t));
            }
        }
        Expr *base = autoderef(value(e->a));
        Type *bt = base->type;
        if (bt->kind != T_RECORD && bt->kind != T_OBJECT && bt->kind != T_CLASS)
            error_at(p, "illegal qualifier: \"%s\" has no members", type_name(bt));
        Look L = {0};
        if (!find_member(bt->st, name, &L.f, &L.m, &L.p))
            error_at(p, "identifier \"%s\" is not a member of \"%s\"", name, bt->st->name);
        return member_ref(base, &L, args, p, name);
    }
    if (args) error_at(p, "expression is not callable");
    return value(e);
}

static Expr *inherited_call(Expr *e) {
    Routine *m = enclosing_method();
    if (!m) error_at(e->pos, "'inherited' is only allowed inside methods");
    Struct *ps = m->owner->parent;
    if (!ps) error_at(e->pos, "\"%s\" has no parent type", m->owner->name);
    const char *name = e->has_id ? e->id.name : m->name;
    Look L = {0};
    if (!find_member(ps, name, &L.f, &L.m, &L.p) || !L.m)
        error_at(e->pos, "inherited method \"%s\" not found in \"%s\"", name, ps->name);
    ExprVec args = {0};
    if (e->has_args) args = e->args;
    else if (!e->has_id)
        for (int i = 0; i < m->params.len; i++) vpush(args, var_expr(m->params.items[i].var, e->pos));
    return make_call(L.m, var_expr(m->self, e->pos), &args, CM_INHERITED, NULL, e->pos);
}

/* ============================================================== expressions */

static Expr *ex(Expr *e) {
    switch (e->kind) {
    case E_INT:
        if (!e->type) e->type = (e->ival >= INT_MIN && e->ival <= INT_MAX) ? t_int : t_i64;
        return e;
    case E_REAL: e->type = t_real; return e;
    case E_STR: if (!e->type) e->type = e->slen == 1 ? t_char : t_str; return e;
    case E_NIL: e->type = t_nil; return e;
    case E_IDENT:
    case E_MEMBER: return resolve_ref(e, NULL);
    case E_CALL:
        if (e->a->kind == E_IDENT || e->a->kind == E_MEMBER) return resolve_ref(e->a, &e->args);
        error_at(e->pos, "expression is not callable");
    case E_INHERITED: return inherited_call(e);
    case E_DEREF: {
        e->a = value(e->a);
        if (e->a->type->kind != T_PTR || !e->a->type->base)
            error_at(e->pos, "typed pointer expected for '^', got \"%s\"", type_name(e->a->type));
        e->type = e->a->type->base;
        return e;
    }
    case E_ADDR: {
        e->a = value(e->a);
        if (!is_lvalue(e->a)) error_at(e->pos, "'@' needs a variable");
        e->type = ptr_to(e->a->type);
        return e;
    }
    case E_INDEX: {
        Expr *a = value(e->a);
        for (int i = 0; i < e->args.len; i++) {
            if (a->type->kind == T_PTR && a->type->base && a->type->base->kind == T_ARRAY) {
                Expr *d = new_expr(E_DEREF, a->pos);
                d->a = a;
                d->type = a->type->base;
                a = d;
            }
            Expr *idx = value(e->args.items[i]);
            Expr *n = new_expr(E_INDEX, e->pos);
            n->a = a;
            if (a->type->kind == T_ARRAY) {
                Type *it = a->type->index;
                if (!is_ordinal(idx->type) || (is_int(it) ? !is_int(idx->type) : !same_type(it, idx->type)))
                    error_at(idx->pos, "incompatible index type: got \"%s\" expected \"%s\"", type_name(idx->type),
                             type_name(it));
                n->type = a->type->base;
            } else if (a->type->kind == T_STR) {
                if (!is_int(idx->type)) error_at(idx->pos, "integer index expected");
                n->type = t_char;
            } else
                error_at(e->pos, "illegal qualifier: \"%s\" cannot be indexed", type_name(a->type));
            vpush(n->args, idx);
            a = n;
        }
        return a;
    }
    case E_UNARY: {
        e->a = value(e->a);
        Type *t = e->a->type;
        if (e->op == OP_NOT) {
            if (t->kind == T_BOOL) e->type = t_bool;
            else if (is_int(t)) e->type = arith_int(t, t);
            else error_at(e->pos, "'not' needs a Boolean or integer operand");
        } else {
            if (!is_numeric(t)) error_at(e->pos, "numeric operand expected");
            e->type = t->kind == T_REAL ? t_real : arith_int(t, t);
        }
        return e;
    }
    case E_BINARY: {
        e->a = value(e->a);
        e->b = value(e->b);
        Type *ta = e->a->type, *tb = e->b->type;
        int op = e->op;
        if (op == OP_ADD && is_strlike(ta) && is_strlike(tb)) {
            e->a = coerce(e->a, t_str, "string concatenation");
            e->b = coerce(e->b, t_str, "string concatenation");
            e->type = t_str;
            return e;
        }
        switch (op) {
        case OP_ADD: case OP_SUB: case OP_MUL:
            if (!is_numeric(ta) || !is_numeric(tb))
                error_at(e->pos, "operator not applicable to \"%s\" and \"%s\"", type_name(ta), type_name(tb));
            if (ta->kind == T_REAL || tb->kind == T_REAL) {
                e->a = coerce(e->a, t_real, "expression");
                e->b = coerce(e->b, t_real, "expression");
                e->type = t_real;
            } else
                e->type = arith_int(ta, tb);
            return e;
        case OP_DIV:
            if (!is_numeric(ta) || !is_numeric(tb)) error_at(e->pos, "'/' needs numeric operands");
            e->a = coerce(e->a, t_real, "expression");
            e->b = coerce(e->b, t_real, "expression");
            e->type = t_real;
            return e;
        case OP_IDIV: case OP_MOD:
            if (!is_int(ta) || !is_int(tb)) error_at(e->pos, "'div'/'mod' need integer operands");
            e->type = arith_int(ta, tb);
            return e;
        case OP_AND: case OP_OR: case OP_XOR:
            if (ta->kind == T_BOOL && tb->kind == T_BOOL) e->type = t_bool;
            else if (is_int(ta) && is_int(tb)) e->type = arith_int(ta, tb);
            else error_at(e->pos, "operator needs Boolean or integer operands");
            return e;
        case OP_SHL: case OP_SHR:
            if (!is_int(ta) || !is_int(tb)) error_at(e->pos, "'shl'/'shr' need integer operands");
            e->type = arith_int(ta, ta);
            return e;
        default: /* comparisons */
            e->type = t_bool;
            if (is_strlike(ta) && is_strlike(tb) && (ta->kind == T_STR || tb->kind == T_STR)) {
                e->a = coerce(e->a, t_str, "comparison");
                e->b = coerce(e->b, t_str, "comparison");
                return e;
            }
            if (is_numeric(ta) && is_numeric(tb)) {
                if (ta->kind == T_REAL || tb->kind == T_REAL) {
                    e->a = coerce(e->a, t_real, "comparison");
                    e->b = coerce(e->b, t_real, "comparison");
                }
                return e;
            }
            if (is_ordinal(ta) && same_type(ta, tb)) return e;
            if (is_ptrlike(ta) && is_ptrlike(tb) && (op == OP_EQ || op == OP_NE)) {
                if (ta->kind == T_NIL || tb->kind == T_NIL || assign_compat(ta, tb) || assign_compat(tb, ta))
                    return e;
            }
            error_at(e->pos, "incompatible types in comparison: \"%s\" and \"%s\"", type_name(ta), type_name(tb));
        }
    }
    case E_IN: {
        e->a = value(e->a);
        if (!is_ordinal(e->a->type)) error_at(e->pos, "ordinal expression expected before 'in'");
        if (e->b->kind != E_SET) error_at(e->b->pos, "set constructor [...] expected after 'in'");
        Expr *s = e->b;
        for (int i = 0; i < s->args.len; i++) {
            for (int k = 0; k < 2; k++) {
                Expr **pp = k ? &s->args2.items[i] : &s->args.items[i];
                if (!*pp) continue;
                *pp = value(*pp);
                Type *it = (*pp)->type;
                if (!(is_int(it) && is_int(e->a->type)) && !same_type(it, e->a->type))
                    error_at((*pp)->pos, "set element type \"%s\" does not match \"%s\"", type_name(it),
                             type_name(e->a->type));
            }
        }
        s->type = t_void;
        e->type = t_bool;
        return e;
    }
    case E_IS: case E_AS: {
        e->a = value(e->a);
        if (e->a->type->kind != T_CLASS) error_at(e->pos, "class instance expected before 'is'/'as'");
        Look L = lookup(e->id.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE || L.sym->type->kind != T_CLASS)
            error_at(e->pos, "class type expected after 'is'/'as'");
        e->ctype = L.sym->type;
        e->type = e->kind == E_IS ? t_bool : e->ctype;
        return e;
    }
    case E_FMT: error_at(e->pos, "format specifiers are only allowed in Write/WriteLn/Str");
    case E_SET: error_at(e->pos, "set constructors are only supported on the right side of 'in'");
    case E_LIST: error_at(e->pos, "initializer list not allowed here");
    default: return e; /* already resolved */
    }
}

static Expr *value(Expr *e) {
    Expr *r = ex(e);
    if (r->kind == E_TYPEREF) error_at(e->pos, "type \"%s\" used as a value", type_name(r->ctype));
    if (r->type == t_void) error_at(e->pos, "procedure call used as a value");
    return r;
}

/* ============================================================== builtins */

static const struct {
    const char *name;
    int id;
} BUILTINS[] = {
    {"Write", BI_WRITE}, {"WriteLn", BI_WRITELN}, {"Read", BI_READ}, {"ReadLn", BI_READLN},
    {"Length", BI_LENGTH}, {"Ord", BI_ORD}, {"Chr", BI_CHR}, {"Succ", BI_SUCC}, {"Pred", BI_PRED},
    {"Inc", BI_INC}, {"Dec", BI_DEC}, {"Abs", BI_ABS}, {"Sqr", BI_SQR}, {"Sqrt", BI_SQRT},
    {"Sin", BI_SIN}, {"Cos", BI_COS}, {"ArcTan", BI_ARCTAN}, {"Exp", BI_EXP}, {"Ln", BI_LN},
    {"Trunc", BI_TRUNC}, {"Round", BI_ROUND}, {"Frac", BI_FRAC}, {"Int", BI_INT}, {"Odd", BI_ODD},
    {"High", BI_HIGH}, {"Low", BI_LOW}, {"SizeOf", BI_SIZEOF}, {"Copy", BI_COPY}, {"Pos", BI_POS},
    {"Delete", BI_DELETE}, {"Insert", BI_INSERT}, {"IntToStr", BI_INTTOSTR}, {"StrToInt", BI_STRTOINT},
    {"FloatToStr", BI_FLOATTOSTR}, {"Str", BI_STR}, {"UpCase", BI_UPCASE}, {"UpperCase", BI_UPPERCASE},
    {"LowerCase", BI_LOWERCASE}, {"Concat", BI_CONCAT}, {"Halt", BI_HALT}, {"Random", BI_RANDOM},
    {"Randomize", BI_RANDOMIZE}, {"Assigned", BI_ASSIGNED}, {"New", BI_NEW}, {"Dispose", BI_DISPOSE},
    {"FreeAndNil", BI_FREEANDNIL}, {"Take", BI_TAKE}, {"SetLength", BI_SETLENGTH},
    {"__ClassName", BI_CLASSNAME}, {"Pi", BI_PI}, {"Exit", BI_EXIT}, {"Break", BI_BREAK},
    {"Continue", BI_CONTINUE}, {NULL, 0}};

static int builtin_id(const char *name) {
    for (int i = 0; BUILTINS[i].name; i++)
        if (STREQ(BUILTINS[i].name, name)) return BUILTINS[i].id;
    return 0;
}
static const char *builtin_name(int id) {
    for (int i = 0; BUILTINS[i].name; i++)
        if (BUILTINS[i].id == id) return BUILTINS[i].name;
    return "?";
}

static int printable(Type *t) {
    return t->kind == T_INT || t->kind == T_REAL || t->kind == T_BOOL || t->kind == T_CHAR ||
           t->kind == T_STR || t->kind == T_ENUM;
}

/* resolve "Init" or "Init(args)" against struct st as ctor/dtor */
static Expr *struct_method_call(Expr *ref, Struct *st, Expr *base, int want_kind, int mode) {
    Ident *id = NULL;
    ExprVec *args = NULL;
    if (ref->kind == E_IDENT) id = &ref->id;
    else if (ref->kind == E_CALL && ref->a->kind == E_IDENT) {
        id = &ref->a->id;
        args = &ref->args;
    } else
        error_at(ref->pos, "constructor/destructor name expected");
    Look L = {0};
    if (!find_member(st, id->name, &L.f, &L.m, &L.p) || !L.m || L.m->kind != want_kind)
        error_at(ref->pos, "\"%s\" is not a %s of \"%s\"", id->name, want_kind == RK_CTOR ? "constructor" : "destructor",
                 st->name);
    Expr *c = make_call(L.m, base, args, mode, NULL, ref->pos);
    return c;
}

static Expr *builtin(int bi, ExprVec *args, Pos p) {
    ExprVec empty = {0};
    if (!args) args = &empty;
    int n = args->len;
    const char *nm = builtin_name(bi);
    Expr *e = new_expr(E_BUILTIN, p);
    e->bi = bi;
    e->type = t_void;
#define NARGS(lo, hi) \
    if (n < (lo) || n > (hi)) error_at(p, "wrong number of arguments for \"%s\"", nm)
#define A(i) (args->items[i])
    switch (bi) {
    case BI_WRITE: case BI_WRITELN:
        for (int i = 0; i < n; i++) {
            Expr *a = A(i);
            if (a->kind == E_FMT) {
                a->a = value(a->a);
                a->b = coerce(value(a->b), t_i64, "field width");
                if (a->c) a->c = coerce(value(a->c), t_i64, "precision");
                a->type = a->a->type;
                if (a->c && a->type->kind != T_REAL) error_at(a->pos, "precision only allowed for real values");
            } else
                a = value(a);
            if (!printable(a->type)) error_at(a->pos, "cannot write a value of type \"%s\"", type_name(a->type));
            vpush(e->args, a);
        }
        return e;
    case BI_READ: case BI_READLN:
        for (int i = 0; i < n; i++) {
            Expr *a = value(A(i));
            check_writable(a, nm);
            Type *t = a->type;
            if (!(is_int(t) || t->kind == T_REAL || t->kind == T_CHAR || t->kind == T_STR))
                error_at(a->pos, "cannot read a value of type \"%s\"", type_name(t));
            vpush(e->args, a);
        }
        return e;
    case BI_LENGTH: {
        NARGS(1, 1);
        Expr *a = value(A(0));
        if (a->type->kind == T_ARRAY) return lit_int(a->type->hi - a->type->lo + 1, t_int, p);
        if (!is_strlike(a->type)) error_at(p, "Length needs a string or array");
        vpush(e->args, a);
        e->type = t_int;
        return e;
    }
    case BI_ORD: case BI_CHR: case BI_SUCC: case BI_PRED: case BI_ODD: {
        NARGS(1, 1);
        Expr *a = value(A(0));
        if (bi == BI_CHR || bi == BI_ODD) {
            if (!is_int(a->type)) error_at(p, "integer expected");
        } else if (!is_ordinal(a->type))
            error_at(p, "ordinal expected");
        vpush(e->args, a);
        e->type = bi == BI_ORD ? (a->type->kind == T_INT ? arith_int(a->type, a->type) : t_int)
                  : bi == BI_CHR ? t_char : bi == BI_ODD ? t_bool : a->type;
        return e;
    }
    case BI_INC: case BI_DEC: {
        NARGS(1, 2);
        Expr *a = value(A(0));
        check_writable(a, nm);
        if (!is_ordinal(a->type)) error_at(p, "ordinal variable expected");
        vpush(e->args, a);
        if (n == 2) {
            Expr *b = value(A(1));
            if (!is_int(b->type)) error_at(p, "integer expected");
            vpush(e->args, b);
        }
        return e;
    }
    case BI_ABS: case BI_SQR: {
        NARGS(1, 1);
        Expr *a = value(A(0));
        if (!is_numeric(a->type)) error_at(p, "numeric argument expected");
        vpush(e->args, a);
        e->type = a->type->kind == T_REAL ? t_real : t_i64;
        return e;
    }
    case BI_SQRT: case BI_SIN: case BI_COS: case BI_ARCTAN: case BI_EXP: case BI_LN: case BI_FRAC:
    case BI_INT: case BI_TRUNC: case BI_ROUND: {
        NARGS(1, 1);
        Expr *a = value(A(0));
        if (!is_numeric(a->type)) error_at(p, "numeric argument expected");
        vpush(e->args, coerce(a, t_real, nm));
        e->type = (bi == BI_TRUNC || bi == BI_ROUND) ? t_i64 : t_real;
        return e;
    }
    case BI_HIGH: case BI_LOW: {
        NARGS(1, 1);
        Expr *a = ex(A(0));
        Type *t = a->kind == E_TYPEREF ? a->ctype : a->type;
        int hi = bi == BI_HIGH;
        switch (t->kind) {
        case T_ARRAY: return lit_int(hi ? t->hi : t->lo, t->index, p);
        case T_STR: return lit_int(hi ? 255 : 0, t_int, p);
        case T_CHAR: return lit_int(hi ? 255 : 0, t_char, p);
        case T_BOOL: return lit_int(hi, t_bool, p);
        case T_ENUM: return lit_int(hi ? t->en->names.len - 1 : 0, t, p);
        case T_INT: {
            long long lo = 0, h = 0;
            switch (t->ik) {
            case IK_I8: lo = -128; h = 127; break;
            case IK_U8: lo = 0; h = 255; break;
            case IK_I16: lo = -32768; h = 32767; break;
            case IK_U16: lo = 0; h = 65535; break;
            case IK_I32: lo = INT_MIN; h = INT_MAX; break;
            case IK_U32: lo = 0; h = 4294967295LL; break;
            case IK_I64: lo = LLONG_MIN; h = LLONG_MAX; break;
            }
            return lit_int(hi ? h : lo, t, p);
        }
        default: error_at(p, "High/Low not applicable to \"%s\"", type_name(t));
        }
    }
    case BI_SIZEOF: {
        NARGS(1, 1);
        Expr *a = ex(A(0));
        e->ctype = a->kind == E_TYPEREF ? a->ctype : a->type;
        e->type = t_int;
        return e;
    }
    case BI_COPY:
        NARGS(3, 3);
        vpush(e->args, coerce(value(A(0)), t_str, nm));
        vpush(e->args, coerce(value(A(1)), t_i64, nm));
        vpush(e->args, coerce(value(A(2)), t_i64, nm));
        e->type = t_str;
        return e;
    case BI_POS:
        NARGS(2, 2);
        vpush(e->args, coerce(value(A(0)), t_str, nm));
        vpush(e->args, coerce(value(A(1)), t_str, nm));
        e->type = t_int;
        return e;
    case BI_DELETE: {
        NARGS(3, 3);
        Expr *s = value(A(0));
        check_writable(s, nm);
        if (s->type->kind != T_STR) error_at(p, "string variable expected");
        vpush(e->args, s);
        vpush(e->args, coerce(value(A(1)), t_i64, nm));
        vpush(e->args, coerce(value(A(2)), t_i64, nm));
        return e;
    }
    case BI_INSERT: {
        NARGS(3, 3);
        vpush(e->args, coerce(value(A(0)), t_str, nm));
        Expr *s = value(A(1));
        check_writable(s, nm);
        if (s->type->kind != T_STR) error_at(p, "string variable expected");
        vpush(e->args, s);
        vpush(e->args, coerce(value(A(2)), t_i64, nm));
        return e;
    }
    case BI_INTTOSTR:
        NARGS(1, 1);
        vpush(e->args, coerce(value(A(0)), t_i64, nm));
        e->type = t_str;
        return e;
    case BI_STRTOINT:
        NARGS(1, 1);
        vpush(e->args, coerce(value(A(0)), t_str, nm));
        e->type = t_int;
        return e;
    case BI_FLOATTOSTR:
        NARGS(1, 1);
        vpush(e->args, coerce(value(A(0)), t_real, nm));
        e->type = t_str;
        return e;
    case BI_STR: {
        NARGS(2, 2);
        Expr *a = A(0);
        if (a->kind == E_FMT) {
            a->a = value(a->a);
            a->b = coerce(value(a->b), t_i64, "field width");
            if (a->c) a->c = coerce(value(a->c), t_i64, "precision");
            a->type = a->a->type;
        } else
            a = value(a);
        if (!is_numeric(a->type)) error_at(p, "Str needs a numeric value");
        Expr *s = value(A(1));
        check_writable(s, nm);
        if (s->type->kind != T_STR) error_at(p, "string variable expected");
        vpush(e->args, a);
        vpush(e->args, s);
        return e;
    }
    case BI_UPCASE: {
        NARGS(1, 1);
        Expr *a = value(A(0));
        if (!is_strlike(a->type)) error_at(p, "UpCase needs a Char or String");
        vpush(e->args, a);
        e->type = a->type;
        return e;
    }
    case BI_UPPERCASE: case BI_LOWERCASE:
        NARGS(1, 1);
        vpush(e->args, coerce(value(A(0)), t_str, nm));
        e->type = t_str;
        return e;
    case BI_CONCAT:
        if (n < 1) error_at(p, "Concat needs arguments");
        for (int i = 0; i < n; i++) vpush(e->args, coerce(value(A(i)), t_str, nm));
        e->type = t_str;
        return e;
    case BI_HALT:
        NARGS(0, 1);
        if (n) vpush(e->args, coerce(value(A(0)), t_i64, nm));
        return e;
    case BI_RANDOM:
        NARGS(0, 1);
        if (n) {
            vpush(e->args, coerce(value(A(0)), t_i64, nm));
            e->type = t_int;
        } else
            e->type = t_real;
        return e;
    case BI_RANDOMIZE: NARGS(0, 0); return e;
    case BI_ASSIGNED: {
        NARGS(1, 1);
        Expr *a = value(A(0));
        if (!is_ptrlike(a->type)) error_at(p, "pointer or class instance expected");
        vpush(e->args, a);
        e->type = t_bool;
        return e;
    }
    case BI_NEW: {
        NARGS(1, 2);
        Expr *a = ex(A(0));
        Type *pt;
        if (a->kind == E_TYPEREF) {
            if (n != 2) error_at(p, "New(Type, Constructor) needs a constructor");
            pt = a->ctype;
            e->type = pt;
        } else {
            if (a->type == t_void) error_at(p, "pointer variable expected");
            check_writable(a, nm);
            pt = a->type;
        }
        if (pt->kind != T_PTR || !pt->base) error_at(p, "New needs a typed pointer");
        vpush(e->args, a);
        if (n == 2) {
            if (pt->base->kind != T_OBJECT) error_at(p, "constructor call in New needs a pointer to an object type");
            e->b = struct_method_call(A(1), pt->base->st, NULL, RK_CTOR, CM_CTOR_NEW);
            e->b->ctype = pt->base;
        }
        return e;
    }
    case BI_DISPOSE: {
        NARGS(1, 2);
        Expr *a = value(A(0));
        if (a->type->kind != T_PTR || !a->type->base) error_at(p, "Dispose needs a typed pointer");
        vpush(e->args, a);
        if (n == 2) {
            if (a->type->base->kind != T_OBJECT) error_at(p, "destructor call in Dispose needs an object pointer");
            Expr *d = new_expr(E_DEREF, p);
            d->a = a;
            d->type = a->type->base;
            e->b = struct_method_call(A(1), a->type->base->st, d, RK_DTOR, -1);
        }
        return e;
    }
    case BI_FREEANDNIL: {
        NARGS(1, 1);
        Expr *a = value(A(0));
        check_writable(a, nm);
        if (a->type->kind != T_CLASS) error_at(p, "FreeAndNil needs a class instance variable");
        vpush(e->args, a);
        return e;
    }
    case BI_TAKE: {
        NARGS(1, 1);
        Expr *a = value(A(0));
        check_writable(a, nm);
        if (!is_owning_type(a->type)) error_at(p, "Take needs a class or typed pointer location");
        vpush(e->args, a);
        e->type = a->type;
        return e;
    }
    case BI_SETLENGTH: {
        NARGS(2, 2);
        Expr *s = value(A(0));
        check_writable(s, nm);
        if (s->type->kind != T_STR) error_at(p, "string variable expected");
        vpush(e->args, s);
        vpush(e->args, coerce(value(A(1)), t_i64, nm));
        return e;
    }
    case BI_CLASSNAME: {
        NARGS(1, 1);
        Expr *a = value(A(0));
        if (a->type->kind != T_CLASS) error_at(p, "class instance expected");
        vpush(e->args, a);
        e->type = t_str;
        return e;
    }
    case BI_PI: {
        NARGS(0, 0);
        Expr *r = new_expr(E_REAL, p);
        r->rval = 3.14159265358979323846;
        r->type = t_real;
        return r;
    }
    case BI_EXIT: case BI_BREAK: case BI_CONTINUE:
        error_at(p, "\"%s\" can only be used as a statement", nm);
    }
    error_at(p, "unsupported builtin \"%s\"", nm);
#undef NARGS
#undef A
}

/* ============================================================== statements */

static Expr *cond(Expr *e) {
    Expr *v = value(e);
    if (v->type->kind != T_BOOL) error_at(e->pos, "Boolean expression expected, got \"%s\"", type_name(v->type));
    return v;
}

static void assign(Stmt *s) {
    Expr *l = s->a;
    if (l->kind == E_IDENT) {
        Look L = lookup(l->id.name);
        if (L.kind == LK_SYM && L.sym->kind == SY_ROUTINE) {
            for (Routine *q = cur; q; q = q->parent)
                if (q == L.sym->rt && q->result) {
                    s->a = var_expr(q->result, l->pos);
                    s->b = coerce(value(s->b), q->result->type, "assignment");
                    return;
                }
        }
    }
    /* property write */
    Expr *base = NULL;
    Look L = {0};
    const char *name = NULL;
    if (l->kind == E_IDENT) {
        L = lookup(l->id.name);
        if (L.kind == LK_MEMBER && L.p) base = var_expr(L.base, l->pos);
        name = l->id.name;
    } else if (l->kind == E_MEMBER) {
        int is_type = 0;
        if (l->a->kind == E_IDENT) {
            Look T = lookup(l->a->id.name);
            is_type = T.kind == LK_SYM && T.sym->kind == SY_TYPE;
        }
        if (!is_type) {
            Expr *b = autoderef(value(l->a));
            if (b->type->kind == T_RECORD || b->type->kind == T_OBJECT || b->type->kind == T_CLASS) {
                if (!find_member(b->type->st, l->id.name, &L.f, &L.m, &L.p))
                    error_at(l->pos, "identifier \"%s\" is not a member of \"%s\"", l->id.name, b->type->st->name);
                if (L.p) base = b;
                else {
                    s->a = member_ref(b, &L, NULL, l->pos, l->id.name);
                    goto normal;
                }
            } else
                error_at(l->pos, "illegal qualifier");
            name = l->id.name;
        }
    }
    if (base && L.p) {
        Prop *pr = L.p;
        if (!pr->write) error_at(l->pos, "property \"%s\" is read-only", name);
        Look W = {0};
        if (!find_member(base->type->st, pr->write, &W.f, &W.m, &W.p) || W.p)
            error_at(pr->pos, "property writer \"%s\" not found", pr->write);
        if (W.f) {
            s->a = member_ref(base, &W, NULL, l->pos, pr->write);
        } else {
            if (W.m->params.len != 1 || W.m->kind != RK_PROC)
                error_at(pr->pos, "property writer \"%s\" must be a procedure with one parameter", pr->write);
            ExprVec args = {0};
            vpush(args, s->b);
            s->kind = S_CALL;
            s->a = make_call(W.m, base, &args, -1, NULL, s->pos);
            s->b = NULL;
            return;
        }
    } else
        s->a = value(l);
normal:
    check_writable(s->a, "assignment");
    s->b = coerce(value(s->b), s->a->type, "assignment");
}

static void stmt(Stmt *s) {
    switch (s->kind) {
    case S_EMPTY: case S_BREAK: case S_CONTINUE: return;
    case S_ASSIGN: assign(s); return;
    case S_CALL: {
        Expr *c = s->a;
        Ident *nm = NULL;
        ExprVec *cargs = NULL;
        if (c->kind == E_IDENT) nm = &c->id;
        else if (c->kind == E_CALL && c->a->kind == E_IDENT) {
            nm = &c->a->id;
            cargs = &c->args;
        }
        if (nm && lookup(nm->name).kind == LK_NONE) {
            int bi = builtin_id(nm->name);
            if (bi == BI_BREAK || bi == BI_CONTINUE) {
                if (cargs && cargs->len) error_at(s->pos, "too many arguments");
                if (!loop_depth) error_at(s->pos, "\"%s\" outside of a loop", nm->name);
                s->kind = bi == BI_BREAK ? S_BREAK : S_CONTINUE;
                return;
            }
            if (bi == BI_EXIT) {
                s->kind = S_EXIT;
                s->a = NULL;
                if (cargs && cargs->len) {
                    if (cargs->len > 1) error_at(s->pos, "too many arguments for Exit");
                    if (!cur || !cur->result) error_at(s->pos, "Exit with a value is only allowed in functions");
                    s->a = coerce(value(cargs->items[0]), cur->result->type, "Exit");
                }
                return;
            }
        }
        Expr *e = ex(c);
        if (e->kind != E_RCALL && e->kind != E_BUILTIN) error_at(s->pos, "illegal expression (statement expected)");
        s->a = e;
        return;
    }
    case S_COMPOUND: stmts(&s->body); return;
    case S_IF:
        s->a = cond(s->a);
        stmt(s->s1);
        if (s->s2) stmt(s->s2);
        return;
    case S_WHILE:
        s->a = cond(s->a);
        loop_depth++;
        stmt(s->s1);
        loop_depth--;
        return;
    case S_REPEAT:
        loop_depth++;
        stmts(&s->body);
        loop_depth--;
        s->a = cond(s->a);
        return;
    case S_FOR: {
        Look L = lookup(s->var.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_VAR)
            error_at(s->var.pos, "loop variable \"%s\" must be a simple variable", s->var.name);
        Var *v = L.sym->var;
        if (!is_ordinal(v->type)) error_at(s->var.pos, "loop variable must be of an ordinal type");
        if (v->level != level || v->is_ref) error_at(s->var.pos, "loop variable must be a local variable");
        if (v->is_const) error_at(s->var.pos, "loop variable \"%s\" is already in use", v->name);
        s->c = var_expr(v, s->var.pos);
        s->a = coerce(value(s->a), v->type, "for loop");
        s->b = coerce(value(s->b), v->type, "for loop");
        v->is_const = 1;
        loop_depth++;
        stmt(s->s1);
        loop_depth--;
        v->is_const = 0;
        return;
    }
    case S_CASE: {
        s->a = value(s->a);
        Type *t = s->a->type;
        if (!is_ordinal(t) && t->kind != T_STR) error_at(s->a->pos, "ordinal or string selector expected");
        for (int i = 0; i < s->arms.len; i++) {
            CaseArm *arm = &s->arms.items[i];
            for (int k = 0; k < arm->lo.len; k++) {
                arm->lo.items[k] = coerce(value(arm->lo.items[k]), t, "case label");
                if (arm->hi.items[k]) arm->hi.items[k] = coerce(value(arm->hi.items[k]), t, "case label");
            }
            stmt(arm->body);
        }
        stmts(&s->else_);
        return;
    }
    case S_WITH: {
        int pushed = 0;
        for (int i = 0; i < s->exprs.len; i++) {
            Expr *v = autoderef(value(s->exprs.items[i]));
            Type *t = v->type;
            if (t->kind != T_RECORD && t->kind != T_OBJECT && t->kind != T_CLASS)
                error_at(v->pos, "record, object or class expected in 'with'");
            if (t->kind != T_CLASS && !is_lvalue(v)) error_at(v->pos, "'with' needs a variable");
            Var *w = new_var("with", t, VK_WITH, v->pos);
            w->is_ref = t->kind != T_CLASS;
            w->borrowed = 1;
            vpush(s->wvars, w);
            s->exprs.items[i] = v;
            push_scope()->with = w;
            pushed++;
        }
        stmt(s->s1);
        while (pushed--) pop_scope();
        return;
    }
    case S_EXIT: return;
    }
}

static void stmts(StmtVec *v) {
    for (int i = 0; i < v->len; i++) stmt(v->items[i]);
}

/* ============================================================== declarations */

static void routine_body(Routine *rt, RoutineDecl *d);

static void add_init(StmtVec *dst, Var *v, Expr *init) {
    if (init->kind == E_LIST) {
        if (v->type->kind != T_ARRAY) error_at(init->pos, "initializer list needs an array type");
        Type *at = v->type;
        if (init->args.len != at->hi - at->lo + 1)
            error_at(init->pos, "wrong number of initializers (expected %lld)", at->hi - at->lo + 1);
        for (int i = 0; i < init->args.len; i++) {
            Stmt *s = new_stmt(S_ASSIGN, init->pos);
            Expr *ix = new_expr(E_INDEX, init->pos);
            ix->a = var_expr(v, init->pos);
            vpush(ix->args, lit_int(at->lo + i, at->index->kind == T_INT ? t_int : at->index, init->pos));
            ix->type = at->base;
            if (init->args.items[i]->kind == E_LIST) error_at(init->pos, "nested initializer lists are not supported");
            s->a = ix;
            s->b = coerce(value(init->args.items[i]), at->base, "initializer");
            vpush(*dst, s);
        }
        return;
    }
    Stmt *s = new_stmt(S_ASSIGN, init->pos);
    s->a = var_expr(v, init->pos);
    s->b = coerce(value(init), v->type, "initializer");
    vpush(*dst, s);
}

static void routine_decl(RoutineDecl *d) {
    Routine *rt = NULL;
    if (d->has_cls) {
        if (cur) error_at(d->pos, "method implementations must be at program level");
        Look L = lookup(d->cls.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE ||
            (L.sym->type->kind != T_CLASS && L.sym->type->kind != T_OBJECT))
            error_at(d->cls.pos, "\"%s\" is not an object or class type", d->cls.name);
        Struct *st = L.sym->type->st;
        for (int i = 0; i < st->methods.len; i++)
            if (STREQ(st->methods.items[i]->name, d->name.name)) rt = st->methods.items[i];
        if (!rt) error_at(d->name.pos, "method \"%s\" is not declared in \"%s\"", d->name.name, st->name);
        if (rt->kind != d->kind) error_at(d->pos, "method kind does not match declaration of \"%s\"", rt->name);
        if (rt->defined) error_at(d->pos, "method \"%s.%s\" is already implemented (or abstract)", st->name, rt->name);
        if (d->forward) error_at(d->pos, "methods cannot be forward declared");
        if (d->has_params) {
            Routine tmp = *rt;
            tmp.params.items = NULL;
            tmp.params.len = tmp.params.cap = 0;
            build_params(&tmp, &d->params);
            if (d->ret) tmp.ret = resolve_type(d->ret);
            if (!same_signature(rt, &tmp))
                error_at(d->pos, "implementation of \"%s.%s\" does not match its declaration", st->name, rt->name);
            for (int i = 0; i < rt->params.len; i++) rt->params.items[i].name = tmp.params.items[i].name;
        }
        routine_body(rt, d);
        return;
    }
    Sym *ex_sym = find_local(d->name.name);
    if (ex_sym && ex_sym->kind == SY_ROUTINE && ex_sym->rt->forward && !ex_sym->rt->defined) {
        rt = ex_sym->rt;
        if (d->forward) error_at(d->pos, "duplicate forward declaration");
        if (d->has_params) {
            Routine tmp = *rt;
            tmp.params.items = NULL;
            tmp.params.len = tmp.params.cap = 0;
            build_params(&tmp, &d->params);
            if (d->ret) tmp.ret = resolve_type(d->ret);
            if (!same_signature(rt, &tmp)) error_at(d->pos, "declaration of \"%s\" differs from forward declaration", rt->name);
            for (int i = 0; i < rt->params.len; i++) rt->params.items[i].name = tmp.params.items[i].name;
        }
    } else {
        if (d->kind == RK_CTOR || d->kind == RK_DTOR)
            error_at(d->pos, "constructors/destructors must be methods");
        rt = new_routine(d->name.name, d->kind, d->name.pos);
        rt->level = level + 1;
        rt->parent = cur;
        rt->cname = cur ? sfmt("%s_%s", cur->cname, d->name.name) : sfmt("R_%s", d->name.name);
        build_params(rt, &d->params);
        if (d->kind == RK_FUNC) {
            if (!d->ret) error_at(d->pos, "function result type expected");
            rt->ret = resolve_type(d->ret);
        }
        rt->ret_borrowed = d->ret_borrowed;
        if (d->ret_borrowed && !is_owning_type(rt->ret))
            error_at(d->pos, "'borrowed' result requires a class or typed pointer type");
        Sym *s = add_sym(SY_ROUTINE, d->name.name, d->name.pos);
        s->rt = rt;
    }
    if (d->forward) {
        rt->forward = 1;
        return;
    }
    routine_body(rt, d);
}

static void routine_body(Routine *rt, RoutineDecl *d) {
    Routine *save_cur = cur;
    int save_level = level, save_loop = loop_depth;
    Scope *save_scope = scope;
    StmtVec *save_init = local_init;
    StmtVec inits = {0};

    cur = rt;
    level = rt->level;
    loop_depth = 0;
    local_init = &inits;
    int has_frame = 0;
    for (int i = 0; i < d->body->decls.len; i++)
        if (d->body->decls.items[i]->kind == D_ROUTINE) has_frame = 1;
    rt->has_frame = has_frame;

    if (rt->owner) {
        Var *self = new_var("Self", rt->owner->type, VK_SELF, rt->pos);
        self->is_ref = rt->owner->kind == SK_OBJECT;
        self->borrowed = 1;
        self->in_frame = has_frame;
        rt->self = self;
        push_scope()->with = self;
    }
    push_scope();
    if (rt->self) add_sym(SY_VAR, "Self", rt->pos)->var = rt->self;
    for (int i = 0; i < rt->params.len; i++) {
        Param *pm = &rt->params.items[i];
        Var *v = new_var(pm->name, pm->type, VK_PARAM, d->pos);
        v->mode = pm->mode;
        v->is_ref = pm->mode == PM_VAR || pm->mode == PM_OUT;
        v->is_const = pm->mode == PM_CONST;
        v->in_frame = has_frame;
        v->borrowed = (pm->mode == PM_VALUE || pm->mode == PM_CONST) && is_owning_type(pm->type);
        pm->var = v;
        add_sym(SY_VAR, pm->name, d->pos)->var = v;
    }
    if (rt->kind == RK_FUNC) {
        Var *r = new_var("Result", rt->ret, VK_RESULT, d->pos);
        r->borrowed = rt->ret_borrowed;
        r->in_frame = has_frame;
        rt->result = r;
        if (!find_local("Result")) add_sym(SY_VAR, "Result", d->pos)->var = r;
    }
    decls(d->body, has_frame);
    stmts(&d->body->body);
    StmtVec body = {0};
    for (int i = 0; i < inits.len; i++) vpush(body, inits.items[i]);
    for (int i = 0; i < d->body->body.len; i++) vpush(body, d->body->body.items[i]);
    rt->body = body;
    rt->defined = 1;

    cur = save_cur;
    level = save_level;
    loop_depth = save_loop;
    scope = save_scope;
    local_init = save_init;
}

static void decls(Block *b, int has_frame) {
    for (int di = 0; di < b->decls.len; di++) {
        Decl *d = b->decls.items[di];
        switch (d->kind) {
        case D_CONST:
            for (int i = 0; i < d->consts.len; i++) {
                ConstDecl *c = &d->consts.items[i];
                if (c->ty) {
                    /* typed constant = initialized static variable */
                    Type *t = resolve_type(c->ty);
                    int save = level;
                    level = 0;
                    Var *v = new_var(c->name.name, t, VK_GLOBAL, c->name.pos);
                    level = save;
                    v->is_global = 1;
                    v->level = 0;
                    if (cur) v->cname = sfmt("C%d_%s", v->id, c->name.name);
                    vpush(prog->globals, v);
                    add_sym(SY_VAR, c->name.name, c->name.pos)->var = v;
                    add_init(&prog->init, v, c->value);
                } else {
                    Expr *lit = const_expr(c->value);
                    Sym *s = add_sym(SY_CONST, c->name.name, c->name.pos);
                    s->cval = lit;
                    s->type = lit->type;
                }
            }
            break;
        case D_TYPE: {
            /* pass 1: pre-register records/objects/classes for forward references */
            for (int i = 0; i < d->types.len; i++) {
                TypeDecl *td = &d->types.items[i];
                TEKind k = td->ty->kind;
                if (k != TE_STRUCT && k != TE_CLASSFWD && k != TE_RECORD) continue;
                Sym *exs = find_local(td->name.name);
                if (exs && exs->kind == SY_TYPE && exs->type->kind == T_CLASS && !exs->type->st->defined &&
                    k == TE_STRUCT && td->ty->sd->kind == SK_CLASS)
                    continue; /* completes forward class */
                int sk = k == TE_RECORD ? SK_RECORD : k == TE_CLASSFWD ? SK_CLASS : td->ty->sd->kind;
                Struct *st = new_struct(td->name.name, sk, td->name.pos);
                if (in_prelude && STREQ(td->name.name, "TObject")) prog->tobject = st;
                add_sym(SY_TYPE, td->name.name, td->name.pos)->type = st->type;
            }
            /* pass 2: define */
            for (int i = 0; i < d->types.len; i++) {
                TypeDecl *td = &d->types.items[i];
                TEKind k = td->ty->kind;
                if (k == TE_CLASSFWD) continue;
                if (k == TE_STRUCT || k == TE_RECORD) {
                    Struct *st = find_local(td->name.name)->type->st;
                    if (k == TE_RECORD) define_record(st, &td->ty->fields);
                    else define_struct(st, td->ty->sd);
                    continue;
                }
                Type *t = resolve_type(td->ty);
                if (t->kind == T_ENUM && !t->name) {
                    t->name = td->name.name;
                    t->en->name = td->name.name;
                }
                add_sym(SY_TYPE, td->name.name, td->name.pos)->type = t;
            }
            for (int i = 0; i < d->types.len; i++) {
                Sym *s = find_local(d->types.items[i].name.name);
                if (s && s->kind == SY_TYPE && s->type->st && !s->type->st->defined)
                    error_at(d->types.items[i].name.pos, "forward class \"%s\" is never defined", s->name);
            }
            break;
        }
        case D_VAR:
            for (int i = 0; i < d->vars.len; i++) {
                VarDecl *vd = &d->vars.items[i];
                Type *t = resolve_type(vd->ty);
                check_complete(t, vd->ty->pos);
                if (vd->borrowed && !is_owning_type(t))
                    error_at(vd->ty->pos, "'borrowed' requires a class or typed pointer type");
                if (vd->init && vd->names.len > 1) error_at(vd->ty->pos, "only one variable can be initialized");
                for (int k = 0; k < vd->names.len; k++) {
                    Ident *id = &vd->names.items[k];
                    Var *v = new_var(id->name, t, cur ? VK_LOCAL : VK_GLOBAL, id->pos);
                    v->borrowed = vd->borrowed;
                    v->in_frame = has_frame;
                    if (cur) vpush(cur->locals, v);
                    else vpush(prog->globals, v);
                    add_sym(SY_VAR, id->name, id->pos)->var = v;
                    if (vd->init) add_init(cur ? local_init : &prog->init, v, vd->init);
                }
            }
            break;
        case D_ROUTINE: routine_decl(d->rt); break;
        }
    }
    for (int i = 0; i < scope->syms.len; i++) {
        Sym *s = scope->syms.items[i];
        if (s->kind == SY_ROUTINE && s->rt->forward && !s->rt->defined)
            error_at(s->rt->pos, "forward declaration of \"%s\" not resolved", s->name);
    }
}

/* ============================================================== program */

static void add_type(const char *name, Type *t) { add_sym(SY_TYPE, name, (Pos){0, 0})->type = t; }

Program *sema_program(Block *prelude, Block *pb, const char *name) {
    prog = NEW(Program);
    prog->name = xstrdup(name);
    t_void = new_type(T_VOID, "untyped");
    t_int = int_type(IK_I32, "Integer");
    t_i64 = int_type(IK_I64, "Int64");
    t_i8 = int_type(IK_I8, "ShortInt");
    t_u8 = int_type(IK_U8, "Byte");
    t_i16 = int_type(IK_I16, "SmallInt");
    t_u16 = int_type(IK_U16, "Word");
    t_u32 = int_type(IK_U32, "Cardinal");
    t_bool = new_type(T_BOOL, "Boolean");
    t_char = new_type(T_CHAR, "Char");
    t_real = new_type(T_REAL, "Real");
    t_str = new_type(T_STR, "String");
    t_ptr = new_type(T_PTR, "Pointer");
    t_nil = new_type(T_NIL, "nil");

    push_scope();
    add_type("Integer", t_int);
    add_type("LongInt", t_int);
    add_type("Int64", t_i64);
    add_type("ShortInt", t_i8);
    add_type("Byte", t_u8);
    add_type("SmallInt", t_i16);
    add_type("Word", t_u16);
    add_type("Cardinal", t_u32);
    add_type("LongWord", t_u32);
    add_type("Boolean", t_bool);
    add_type("Char", t_char);
    add_type("Real", t_real);
    add_type("Double", t_real);
    add_type("Single", t_real);
    add_type("Extended", t_real);
    add_type("String", t_str);
    add_type("Pointer", t_ptr);
    Sym *s = add_sym(SY_CONST, "True", (Pos){0, 0});
    s->cval = lit_int(1, t_bool, (Pos){0, 0});
    s = add_sym(SY_CONST, "False", (Pos){0, 0});
    s->cval = lit_int(0, t_bool, (Pos){0, 0});
    s = add_sym(SY_CONST, "MaxInt", (Pos){0, 0});
    s->cval = lit_int(INT_MAX, t_int, (Pos){0, 0});

    in_prelude = 1;
    decls(prelude, 0);
    in_prelude = 0;

    push_scope();
    decls(pb, 0);
    stmts(&pb->body);
    prog->body = pb->body;

    for (int i = 0; i < prog->routines.len; i++) {
        Routine *r = prog->routines.items[i];
        if (r->owner && !r->defined)
            error_at(r->pos, "method \"%s.%s\" is declared but not implemented", r->owner->name, r->name);
    }
    return prog;
}
