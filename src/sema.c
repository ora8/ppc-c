/* sema.c - semantic analysis: name resolution, typing, OOP layout.
 * Produces a resolved/annotated AST that the borrow checker and the
 * code generator work on. Identifiers are case-sensitive. */
#include "ast.h"
#include <ctype.h>
#include <limits.h>

typedef enum { SY_VAR, SY_CONST, SY_TYPE, SY_ROUTINE, SY_GENERIC, SY_VARIANT } SymKind; /* SY_VARIANT: type, vidx */

/* a generic type: parsed again for every instance (monomorphization) */
typedef struct {
    Struct *st;
    Type **args;
} Instance;
typedef struct {
    char *name;
    IdentVec params;
    void *toks;
    int tok;
    Pos pos;
    VEC(char *) keys;
    VEC(Instance) insts;
    struct Scope *scope;      /* where the generic is declared: instances are made there */
    VEC(RoutineDecl *) impls; /* method implementations, parsed again per instance */
    ConsVec cons;             /* constraints per type parameter */
    int is_routine;           /* generic function/procedure: Max<T> */
    RoutineDecl *rdecl;       /* its first parse (parameter types for inference) */
    VEC(Routine *) rts;       /* routine instances, parallel to keys */
    VEC(char *) akeys;        /* instances made while generics are checked with placeholders */
    VEC(Instance) ainsts;
    VEC(Routine *) arts;
    RoutineDecl *arch_rd;     /* routine: its placeholder instance (body checked once) */
    Routine *arch_rt;
    int broken;               /* the generic itself has errors: its instances are not checked again */
    int iface_only;           /* generic routine declared in a module interface, defined in its implementation */
    Struct *owner;            /* generic method: its class or record */
    MethodDecl *mdecl;        /* generic method: its declaration in the type */
} Generic;
static VEC(Generic *) all_generics;
static int cur_gen_id;           /* the generic whose instance is being made */
static VEC(int) gen_stack;
static Generic *self_check; /* the generic being checked on its own (stage 4) */
static struct Scope *iface_scope; /* module implementation: the interface scope whose headers it defines */
static int in_iface;              /* module interface: headers are defined later, no 'not resolved' check yet */

typedef struct {
    SymKind kind;
    char *name;
    Generic *gen;
    Var *var;
    Type *type;
    Expr *cval;
    Routine *rt;
    int vidx;
} Sym;

typedef struct Scope {
    VEC(Sym *) syms;
    Struct *cls; /* class method body: other class methods by bare name */
    struct Scope *up;
    Var *self_obj; /* method body: members of Self are found without 'Self.' */
} Scope;

enum { LK_NONE, LK_SYM, LK_MEMBER };
typedef struct {
    int kind;
    Sym *sym;
    Var *base;
    Field *f;
    Routine *m;
    Prop *p;
    Generic *gm; /* a generic method of Self */
} Look;
static void check_visible(Look *L, Pos p, const char *name);
static Type *classref_of(Struct *st);
static Expr *classref_member(Expr *cref, const char *name, ExprVec *args, Pos p);
static Field *find_cfield(Struct *st, const char *name);
static Look cfield_look(Field *f);
static Expr *cfield_ref(Field *f, Pos p);
static void class_fields(Struct *s, FieldDecl *fd, Type *t);
static int set_elem_ok(Type *st, Type *t);
static Expr *set_binary(Expr *e);
static Expr *set_literal(Expr *e, Type *to);
static Type *set_of_elems(Expr *e);
static int hashable(Type *t);
static Expr *cast_expr(Type *t, ExprVec *args, Pos p);
static void operator_rules(Struct *s, MethodDecl *md);
static void operator_check(Struct *s, Routine *r);
static int arg_cost(Param *pm, Expr *a);
static Expr *user_operator(Expr *e, int op, Expr *a, Expr *b);
static int has_operator(Type *t, const char *nm);
static Expr *autoderef(Expr *e);
static Generic *find_gmethod(Struct *st, const char *name);
static Expr *gmethod_call(Generic *g, Expr *base, TypeExprVec *targs, ExprVec *args, Pos p);
static void generic_method(Struct *s, MethodDecl *md);
static void gmethod_impl(Generic *g);
static Routine *infer_routine(Generic *g, ExprVec *args, Pos p);

static Program *prog;
static Scope *scope;
static Routine *cur;
static int level;
static int loop_depth;
static int in_prelude;
static int archetype_mode; /* > 0 while a generic is checked with placeholder types (no code is produced) */
static struct Scope *gscope;   /* program scope: generic instances are made there */
static VEC(char *) broken_names; /* variables whose declaration had an error: their uses are not reported again */
static Struct *inst_cls;       /* method body of a generic instance being checked */
static int generic_depth;      /* > 0 while a generic instance is made or checked */
typedef struct {
    Generic *g;
    Instance in;
    RoutineDecl *impl;
    Routine *rt;    /* instance of a generic routine: its body */
    RoutineDecl *rd;
    char *key;
} PendingBody;
static const char *inst_cname; /* C name of the generic routine instance being declared */
static void check_constraints(Generic *g, Type **args, Pos p);
static void validate_constraints(Generic *g);
static VEC(PendingBody) pending_bodies;
static int var_counter, routine_counter, struct_counter, enum_counter;
static StmtVec *local_init;
static int in_block; /* inside a block statement with its own declarations */

static Type *t_void, *t_int, *t_i64, *t_i8, *t_u8, *t_i16, *t_u16, *t_u32, *t_bool, *t_char,
    *t_real, *t_str, *t_ptr, *t_nil, *t_lit;

static Expr *value(Expr *e);
static Expr *try_expr(Expr *e);
/* a result that nobody holds yet: a function call */
int res_temp(Expr *e) {
    while (e->kind == E_CONV) e = e->a;
    return e->kind == E_RCALL && e->rt->kind == RK_FUNC && !e->rt->ret_ref && !e->rt->ret_borrowed;
}
static struct Scope *top_scope, *lambda_outer;
static Expr *try_site;
static Expr *routine_ref(Routine *rt, Pos p);
static Expr *indirect_call(Expr *callee, ExprVec *args, Pos p);
static Expr *lambda_expr(Expr *e);
static void for_in(Stmt *s);
static Expr *variant_value(Type *t, int vi, ExprVec *args, Pos p);
static char *variant_list(Struct *st);
static Expr *variant_test(Expr *e);
static void sum_case(Stmt *s, int from_if);
static Expr *mk_ident(const char *name, Pos p);
static Expr *mk_bin(int op, Expr *a, Expr *b, Pos p);
#define l_pos(arm) ((arm)->body->pos)
static void stmt(Stmt *s);
static Type *proc_type(Routine *sig);
static Routine *enumerator_of(Struct *st);
static void for_enumerator(Stmt *s, Expr *coll, Routine *ge);
static Expr *forin_call; /* the GetEnumerator call made by 'for ... in' (it may refer to its container) */
static void no_closure(Type *t, Pos p, const char *where);
static int is_aggregate(Type *t);
static int same_agg(Type *a, Type *b);
static int equatable(Type *t);
static void closure_of(Type *t);
static Expr *method_value(Routine *m, Expr *base, int mode, Pos p);
static void no_method_value(Expr *e);
static void move_check(Expr *src, const char *ctx);
static Expr *param_binary(Expr *e);
static Expr *ex(Expr *e);
static Type *resolve_type(TypeExpr *te);
static void decls(Block *b, int has_frame);
static void stmts(StmtVec *v);
static void decl1(Decl *d, int has_frame);
static void decls_tail(Block *b);
static void module_decls(Block *m);
static Type *infer_type(Expr *r, Pos p, const char *name);
static void settle_var(Var *v, Type *t, Expr *src, Pos p);
static void check_settled(Var **vars, int n);

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

static const char *res_name(Type *base, Type *err) {
    const char *b = base ? sfmt("result %s", type_name(base)) : "result";
    return err->kind == T_STR ? b : sfmt("%s of %s", b, type_name(err));
}

static int same_signature(Routine *a, Routine *b);
static Routine *pick_overload(Routine *rt, Expr *base, ExprVec *args, int mode, Type *newcls, Pos p);
static int in_overloads(Routine *first, Routine *r);
static Routine *find_routine_sym(const char *name);
static char *sig_text(Routine *r);
static int same_params(Routine *a, Routine *b);
static int method_overload(Struct *s, MethodDecl *md, const char *what);
static void overload_unique(Struct *s, Routine *r);
/* function(Integer, Integer): Integer */
static const char *proc_name(Routine *r) {
    Buf b = {0};
    buf_put(&b, r->kind == RK_FUNC ? "function(" : "procedure(");
    for (int i = 0; i < r->params.len; i++) {
        Param *p = &r->params.items[i];
        buf_printf(&b, "%s%s%s", i ? ", " : "", p->mode == PM_VAR ? "var " : p->mode == PM_OWNED ? "vao " : "", type_name(p->type));
    }
    buf_put(&b, ")");
    if (r->ret) buf_printf(&b, ": %s", type_name(r->ret));
    return xstrdup(buf_str(&b));
}

const char *type_name(Type *t) {
    if (!t) return "?";
    if (t->name) return t->name;
    switch (t->kind) {
    case T_VOID: return "untyped";
    case T_SET: return "set";
    case T_CLASSREF: return "class reference";
    case T_INT: return "Integer";
    case T_BOOL: return "Boolean";
    case T_CHAR: return "Char";
    case T_REAL: return "Real";
    case T_STR: return "String";
    case T_NIL: return "Pointer(nil)";
    case T_PTR: return t->base ? sfmt("^%s", type_name(t->base)) : "Pointer";
    case T_ARRAY: return sfmt("array[%lld..%lld] of %s", t->lo, t->hi, type_name(t->base));
    case T_RECORD: case T_CLASS: return t->st->name;
    case T_ENUM: return t->en->name;
    case T_OPT: return sfmt("optional %s", type_name(t->base));
    case T_RES: return res_name(t->base, t->err);
    case T_LIT: return "literal";
    case T_PARAM: return "type parameter";
    case T_DYN: return sfmt("array of %s", type_name(t->base));
    case T_PROC: return t->name ? t->name : proc_name(t->proc);
    }
    return "?";
}

static int is_int(Type *t) { return t->kind == T_INT; }
static int is_ordinal(Type *t) {
    return t->kind == T_INT || t->kind == T_CHAR || t->kind == T_BOOL || t->kind == T_ENUM;
}
static int is_numeric(Type *t) { return t->kind == T_INT || t->kind == T_REAL; }
static int is_strlike(Type *t) { return t->kind == T_STR || t->kind == T_CHAR; }
static int is_ptrlike(Type *t) { return t->kind == T_PTR || t->kind == T_CLASS || t->kind == T_NIL || t->kind == T_CLASSREF; }

/* owning types: class instances and typed pointers (borrow checker) */
int g_param_owning;
/* while a generic body is checked on its own, a type parameter that may be an object is one */
static int param_owner(Type *t) { return g_param_owning && t && t->kind == T_PARAM && !(t->ik & (PC_COPY | PC_CMP | PC_NUM | PC_HASH)); }
int is_ref_type(Type *t) { return t && (t->kind == T_CLASS || (t->kind == T_PTR && t->base) || param_owner(t)); }
/* a value that contains strings (each has its own heap buffer: copied on assignment, freed at the end) */
int has_str(Type *t) {
    if (!t) return 0;
    switch (t->kind) {
    case T_STR: return 1;
    case T_ARRAY: case T_DYN: case T_OPT: return has_str(t->base);
    case T_RES: return has_str(t->base) || has_str(t->err);
    case T_RECORD:
        for (int i = 0; i < t->st->fields.len; i++)
            if (has_str(t->st->fields.items[i]->type)) return 1;
        return 0;
    default: return 0;
    }
}

/* a value (array, record, optional value) that owns objects in its elements or fields */
int has_owned(Type *t) {
    if (!t) return 0;
    switch (t->kind) {
    case T_DYN: return 1; /* owns its buffer */
    case T_ARRAY: return is_ref_type(t->base) || has_owned(t->base);
    case T_OPT: return has_owned(t->base);
    case T_RES: return (t->base && (is_ref_type(t->base) || has_owned(t->base))) || (t->err && has_owned(t->err));
    case T_RECORD:
        for (int i = 0; i < t->st->fields.len; i++) {
            Field *f = t->st->fields.items[i];
            if ((is_ref_type(f->type) && !f->borrowed) || has_owned(f->type)) return 1;
        }
        return 0;
    default: return 0;
    }
}
/* an expression that yields a freshly owned object nobody holds yet */
int is_owning_temp(Expr *e) {
    while (e->kind == E_CONV || e->kind == E_CAST || e->kind == E_AS) e = e->a;
    if (e->kind == E_TRY) return is_ref_type(e->type); /* 'try Open(...)': the object of a fresh result */
    if (e->kind == E_RCALL) {
        Routine *r = e->rt;
        if (r->kind == RK_CTOR) return e->cmode == CM_NEWCLASS;
        return r->kind == RK_FUNC && !r->ret_borrowed &&
               (is_ref_type(r->ret) || (r->ret && r->ret->kind == T_OPT && is_ref_type(r->ret->base)));
    }
    if (e->kind == E_BUILTIN) return e->bi == BI_TAKE || e->bi == BI_BOXNEW || (e->bi == BI_NEW && e->type && is_ref_type(e->type));
    return 0;
}

int is_owning_type(Type *t) {
    return t && (t->kind == T_CLASS || (t->kind == T_PTR && t->base) || param_owner(t) || (t->kind == T_OPT && param_owner(t->base)));
}

/* d is b, a descendant of b, or (b an interface) implements b */
int struct_descends(Struct *d, Struct *b) {
    if (d->is_iface && prog && b == prog->tobject) return 1; /* every object is a TObject */
    for (Struct *c = d; c; c = c->parent) {
        if (c == b) return 1;
        if (b->is_iface)
            for (int i = 0; i < c->ifaces.len; i++)
                if (struct_descends(c->ifaces.items[i], b)) return 1;
    }
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
    case T_RECORD: case T_CLASS: return a->st == b->st;
    case T_ENUM: return a->en == b->en;
    case T_OPT: return same_type(a->base, b->base);
    case T_PARAM: return 0; /* two placeholders are only the same if they are one object */
    case T_DYN: return same_type(a->base, b->base);
    case T_PROC: return a->closure == b->closure && same_signature(a->proc, b->proc);
    case T_SET: return a->lo == b->lo && a->hi == b->hi && (same_type(a->base, b->base) || (is_int(a->base) && is_int(b->base)));
    case T_CLASSREF: return a->st == b->st;
    case T_RES:
        if (!a->base || !b->base) return !a->base && !b->base && same_type(a->err, b->err);
        return same_type(a->base, b->base) && same_type(a->err, b->err);
    default: return 1;
    }
}

static int is_optional(Type *t) { return t->kind == T_OPT || (t->kind == T_CLASS && t->nilable); }

/* the type inside an optional */
/* 'const T': the same class type, but the object can only be read through it */
static Type *readonly_of(Type *t) {
    if (t->kind != T_CLASS || t->readonly) return t;
    Type *r = NEW(Type);
    *r = *t;
    r->readonly = 1;
    r->name = sfmt("%s const", type_name(t));
    return r;
}

/* 'T const' for any type (values: unchangeable, passed by reference as parameter) */
static Type *readonly_any(Type *t) {
    if (t->kind == T_CLASS) return readonly_of(t);
    if (t->readonly) return t;
    Type *r = NEW(Type);
    *r = *t;
    r->readonly = 1;
    r->name = sfmt("%s const", type_name(t));
    return r;
}

static Type *unwrapped(Type *t) {
    if (t->kind == T_OPT) return t->base;
    return t->readonly ? readonly_of(t->st->type) : t->st->type; /* class: the non-optional class type */
}

static Type *arith_int(Type *a, Type *b) {
    if ((a->kind == T_INT && (a->ik == IK_I64 || a->ik == IK_U32)) ||
        (b->kind == T_INT && (b->ik == IK_I64 || b->ik == IK_U32)))
        return t_i64;
    return t_int;
}

/* 0 = incompatible, 1 = compatible as is, 2 = needs conversion node */
static int assign_compat(Type *to, Type *from) {
    if (to->kind == T_CLASSREF && from->kind == T_CLASSREF && struct_descends(from->st, to->st)) return 2; /* TCircle as a TShapeClass */
    if (to->kind == T_PROC && from->kind == T_PROC && to->closure && !from->closure && same_signature(to->proc, from->proc))
        return 2; /* a plain routine is a closure without captures */
    if (to->kind == T_CLASS && from->kind == T_CLASS && from->nilable && !to->nilable) return 0;
    if (to->kind == T_CLASS && from->kind == T_CLASS && from->readonly && !to->readonly) return 0;
    if (to->kind == T_OPT) {
        if (from->kind == T_OPT) return same_type(to, from);
        if (from->kind == T_NIL) return 2;
        return assign_compat(to->base, from) ? 2 : 0;
    }
    if (to->kind == T_RES) { /* a value of T is a successful result */
        if (from->kind == T_RES) return same_type(to, from);
        return to->base && assign_compat(to->base, from) ? 2 : 0;
    }
    if (same_type(to, from)) return 1;
    switch (to->kind) {
    case T_INT: return from->kind == T_INT;
    case T_REAL: return from->kind == T_INT ? 2 : 0;
    case T_STR: return from->kind == T_CHAR ? 2 : 0;
    case T_PTR:
        if (from->kind == T_NIL) return 1;
        if (from->kind == T_PTR && (!to->base || !from->base)) return 2;
        return 0;
    case T_CLASS:
        if (from->kind == T_NIL) return to->nilable;
        if (from->kind == T_CLASS && struct_descends(from->st, to->st)) return 2;
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

static Expr *value(Expr *e);
static int find_member(Struct *st, const char *name, Field **f, Routine **m, Prop **p);
static Expr *coerce(Expr *e, Type *to, const char *ctx);

/* (X: 1; Y: 2), (1, 2, 3) and default get their type from where they are used */
static Expr *literal_to(Expr *e, Type *to, const char *ctx) {
    if (e->kind == E_DEFAULT) {
        if (to->kind == T_PROC)
            error_at(e->pos, "'default' would be no routine, but \"%s\" needs one (give a routine or a lambda, or declare it 'optional')",
                     type_name(to));
        if (to->kind == T_CLASS && !to->nilable)
            error_at(e->pos, "'default' would be no object, but \"%s\" needs one (create it, or declare it 'optional')",
                     type_name(to));
        e->type = to;
        return e;
    }
    if (e->kind == E_OK || e->kind == E_ERR) {
        if (to->kind != T_RES)
            error_at(e->pos, "'%s' makes a result, but \"%s\" is expected in %s", e->kind == E_OK ? "ok" : "error",
                     type_name(to), ctx);
        if (e->kind == E_ERR) e->a = coerce(value(e->a), to->err, "error(...)");
        else if (to->base) {
            if (!e->a) error_at(e->pos, "'ok' needs a value for \"%s\": 'ok(v)'", type_name(to));
            e->a = coerce(value(e->a), to->base, "ok(...)");
        } else if (e->a)
            error_at(e->pos, "\"%s\" has no value: write 'ok' without an argument", type_name(to));
        e->type = to;
        return e;
    }
    if (to->kind == T_OPT) return coerce(literal_to(e, to->base, ctx), to, ctx);
    if (to->kind == T_RES) {
        if (!to->base) error_at(e->pos, "\"%s\" has no value: use 'ok' or 'error(...)'", type_name(to));
        return coerce(literal_to(e, to->base, ctx), to, ctx);
    }
    if (e->kind == E_LIST && to->kind == T_DYN) { /* (1, 2, 3) for 'array of T': any length */
        for (int i = 0; i < e->args.len; i++) e->args.items[i] = coerce(value(e->args.items[i]), to->base, "array literal");
        e->type = to;
        return e;
    }
    if (e->kind == E_LIST) {
        if (to->kind != T_ARRAY) error_at(e->pos, "an array literal cannot be used as \"%s\" in %s", type_name(to), ctx);
        long long n = to->hi - to->lo + 1;
        if (e->args.len != n) error_at(e->pos, "array literal has %d elements, \"%s\" needs %lld", e->args.len, type_name(to), n);
        for (int i = 0; i < e->args.len; i++) e->args.items[i] = coerce(value(e->args.items[i]), to->base, "array literal");
        e->type = to;
        return e;
    }
    if (e->kind == E_SET) {
        if (to->kind != T_SET) error_at(e->pos, "a set [...] cannot be used as \"%s\" in %s", type_name(to), ctx);
        return set_literal(e, to);
    }
    /* E_RECLIT */
    if (to->kind != T_RECORD) error_at(e->pos, "a record literal cannot be used as \"%s\" in %s", type_name(to), ctx);
    for (int i = 0; i < e->fnames.len; i++) {
        Field *f;
        Routine *m;
        Prop *pr;
        if (!find_member(to->st, e->fnames.items[i].name, &f, &m, &pr) || !f)
            error_at(e->fnames.items[i].pos, "\"%s\" has no field \"%s\"", type_name(to), e->fnames.items[i].name);
        if (f->vis != VIS_PUBLIC) {
            Look V = {0};
            V.f = f;
            check_visible(&V, e->fnames.items[i].pos, f->name);
        }
        for (int j = 0; j < i; j++)
            if (STREQ(e->fnames.items[j].name, e->fnames.items[i].name))
                error_at(e->fnames.items[i].pos, "field \"%s\" is given twice", e->fnames.items[i].name);
        e->args.items[i] = coerce(value(e->args.items[i]), f->type, sfmt("field \"%s\"", f->name));
    }
    e->type = to; /* fields that are not given get their default (zero) value */
    return e;
}

static Expr *coerce(Expr *e, Type *to, const char *ctx) {
    if (to->kind == T_PROC && e->kind == E_RCALL && e->cmode == CM_NORMAL && !e->a && !e->args.len && !e->rt->params.len &&
        !e->rt->is_sig)
        e = routine_ref(e->rt, e->pos); /* 'f := Tick': the routine itself, not a call */
    if (to->kind == T_PROC && e->kind == E_RCALL && e->a && !e->args.len && !e->rt->params.len && e->rt->owner &&
        (e->cmode == CM_STATIC || e->cmode == CM_VIRTUAL || e->cmode == CM_IFACE))
        e = method_value(e->rt, e->a, e->cmode, e->pos); /* Visit(obj.Show): the method of obj, not a call */
    if (to->kind == T_PARAM && (to->ik & PC_NUM) && (e->kind == E_INT || e->kind == E_REAL)) {
        e->type = to; /* a number literal is a value of every numeric type */
        return e;
    }
    if (e->type && e->type->kind == T_LIT) return literal_to(e, to, ctx);
    int c = assign_compat(to, e->type);
    if (!c && e->type->kind == T_NIL)
        error_at(e->pos, "none can only be stored in an optional type, not in \"%s\" (declare it 'optional %s')",
                 type_name(to), type_name(to));
    if (!c && e->type->kind == T_CLASS && to->kind == T_CLASS && e->type->readonly && !to->readonly &&
        !(e->type->nilable && !to->nilable))
        error_at(e->pos, "read-only reference (\"%s\") cannot be used as a changeable \"%s\" in %s", type_name(e->type),
                 type_name(to), ctx);
    if (!c && is_optional(e->type) && !is_optional(to))
        error_at(e->pos, "optional value of type \"%s\" cannot be used as \"%s\" in %s; unpack it first: 'if x is some v then ...'",
                 type_name(e->type), type_name(to), ctx);
    if (!c && e->type->kind == T_RES)
        error_at(e->pos, "result of type \"%s\" cannot be used as \"%s\" in %s; unpack it first: 'if r is ok v then ...', 'case r of ok v: ...; error e: ... end' or 'try r'",
                 type_name(e->type), type_name(to), ctx);
    if (c && to->kind == T_RES && e->type->kind != T_RES) {
        Expr *inner = coerce(e, to->base, ctx);
        Expr *w = new_expr(E_CONV, e->pos);
        w->a = inner;
        w->type = to;
        return w;
    }
    if (c && to->kind == T_OPT && e->type->kind != T_OPT && e->type->kind != T_NIL) {
        Expr *inner = coerce(e, to->base, ctx); /* e.g. Integer -> optional Real */
        Expr *w = new_expr(E_CONV, e->pos);
        w->a = inner;
        w->type = to;
        return w;
    }
    if (!c) {
        if (to->kind == T_PROC && !to->closure && e->kind == E_METHREF)
            error_at(e->pos, "a method value refers to its object and cannot be stored as \"%s\" in %s; declare the parameter 'reference to %s'",
                     type_name(to), ctx, type_name(to));
        if (to->kind == T_PROC && !to->closure && e->type->kind == T_PROC && e->type->closure)
            error_at(e->pos, "this %s uses variables of the routine around it (a closure) and cannot be stored as \"%s\" in %s; declare the parameter or variable 'reference to %s'",
                     e->kind == E_LAMBDA ? "lambda" : "routine", type_name(to), ctx, type_name(to));
        if (to->kind == T_CLASS && to->st->is_iface && e->type->kind == T_CLASS && !e->type->st->is_iface)
            error_at(e->pos, "\"%s\" does not implement the interface \"%s\" in %s (declare it: '%s = class(..., %s)')",
                     type_name(e->type), type_name(to), ctx, e->type->st->name, to->st->name);
        error_at(e->pos, "incompatible types in %s: got \"%s\" expected \"%s\"", ctx, type_name(e->type),
                 type_name(to));
    }
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

static Sym *find_in(Scope *sc, const char *name) {
    for (int i = 0; i < sc->syms.len; i++)
        if (STREQ(sc->syms.items[i]->name, name)) return sc->syms.items[i];
    return NULL;
}
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
    if (st->is_iface && prog->tobject) { /* an interface value is an object: ClassName, Free */
        Struct *o = prog->tobject;
        for (int i = 0; i < o->methods.len; i++)
            if (STREQ(o->methods.items[i]->name, name) && o->methods.items[i]->kind != RK_CTOR) {
                *m = o->methods.items[i];
                return 1;
            }
    }
    return 0;
}

static Pos scope_pos;
static void classm_rules(MethodDecl *md);
static void auto_rules(MethodDecl *md, Struct *s, Routine *r);
static Look lookup(const char *name) {
    Look L = {0};
    for (Scope *s = scope; s; s = s->up) {
        if (s->self_obj) {
            Field *f;
            Routine *m;
            Prop *p;
            if (find_member(s->self_obj->type->st, name, &f, &m, &p)) {
                L.kind = LK_MEMBER;
                L.base = s->self_obj;
                L.f = f;
                L.m = m;
                L.p = p;
                return L;
            }
            Field *cf = find_cfield(s->self_obj->type->st, name);
            if (cf) return cfield_look(cf);
            Generic *gm = find_gmethod(s->self_obj->type->st, name);
            if (gm) {
                L.kind = LK_MEMBER;
                L.base = s->self_obj;
                L.gm = gm;
                return L;
            }
        }
        if (s->cls) {
            Field *f;
            Routine *m;
            Prop *p;
            Field *cf = find_cfield(s->cls, name);
            if (cf) return cfield_look(cf);
            if (find_member(s->cls, name, &f, &m, &p)) {
                if (!m || !m->is_classm)
                    error_at(scope_pos, "'%s' is not available in a class method (a class method has no Self)", name);
                L.kind = LK_MEMBER;
                L.m = m;
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
    case E_CAST: return e->type->kind == T_RECORD && is_lvalue(e->a);
    default: return 0;
    }
}

/* is e (a part of) a value stored in a constant? Objects behind a class reference stay changeable. */
static int rooted_const(Expr *e) {
    switch (e->kind) {
    case E_VAR: return e->var->is_const && e->var->type->kind != T_CLASS;
    case E_FIELD:
        if (e->a->type->kind == T_CLASS) /* object fields: read-only behind a 'const T' reference */
            return e->a->type->readonly || (e->a->kind == E_VAR && e->a->var->vkind == VK_SELF && e->a->var->is_const);
        return rooted_const(e->a);
    case E_INDEX: case E_CAST: case E_CONV: return rooted_const(e->a);
    case E_DEREF: return e->a->kind == E_VAR && e->a->var->is_const;
    default: return 0;
    }
}

static const char *const_root_name(Expr *e) {
    while (e && e->kind != E_VAR) e = e->a;
    return e ? e->var->name : "expression";
}

/* the variable an lvalue/receiver expression is rooted in */
static Var *root_var(Expr *e) {
    while (e && e->kind != E_VAR) {
        if (e->kind != E_FIELD && e->kind != E_INDEX && e->kind != E_DEREF && e->kind != E_CAST && e->kind != E_CONV) return NULL;
        e = e->a;
    }
    return e ? e->var : NULL;
}

/* innermost enclosing routine declared '; const;' that is not a method (no side effects allowed) */
static Routine *pure_ctx(void) {
    for (Routine *r = cur; r; r = r->parent)
        if (r->is_constm && !r->owner) return r;
    return NULL;
}

static int nonlocal_to_rt(Var *v, Routine *r) { return v && (v->is_global || v->level < r->level); }

static void check_writable(Expr *e, const char *what) {
    if (!is_lvalue(e)) error_at(e->pos, "%s: variable identifier expected", what);
    if (e->kind == E_VAR && e->var->is_stack)
        error_at(e->pos, "%s: \"%s\" is a stack object and cannot be reassigned (it lives in place until the end of its block; use 'vao' for an object that can be replaced)",
                 what, e->var->name);
    Routine *pr = pure_ctx();
    if (pr && nonlocal_to_rt(root_var(e), pr))
        error_at(e->pos, "%s: const %s '%s' cannot change the non-local variable '%s'", what,
                 pr->kind == RK_FUNC ? "function" : "procedure", pr->name, root_var(e)->name);
    if (e->kind != E_VAR && rooted_const(e))
        error_at(e->pos, "%s: cannot modify a part of '%s', which is constant", what, const_root_name(e));
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
    s->type = new_type(kind == SK_RECORD ? T_RECORD : T_CLASS, name);
    s->type->st = s;
    return s;
}

/* zero-initialized storage (arrays, records, New(p)^) cannot hold non-nilable references */
static void require_nilable(Type *t, Pos p, const char *where) {
    if (t->kind == T_PROC)
        error_at(p, "%s of type \"%s\" must be declared 'optional %s' (this storage starts out empty)", where, type_name(t),
                 type_name(t));
    if (g_nil_check && is_ref_type(t) && !t->nilable && t->kind != T_PARAM) /* T: checked for each instance */
        error_at(p, "%s of type \"%s\" must be declared 'optional %s' (this storage starts out empty)", where, type_name(t),
                 type_name(t));
}

static void check_complete(Type *t, Pos p) {
    if (t->kind == T_RECORD && !t->st->defined)
        error_at(p, "type \"%s\" is not completely defined", t->st->name);
}

static void add_fields(Struct *s, FieldDeclVec *fields) {
    for (int i = 0; i < fields->len; i++) {
        FieldDecl *fd = &fields->items[i];
        Type *t = resolve_type(fd->ty);
        no_closure(t, fd->ty->pos, "a field");
        check_complete(t, fd->ty->pos);
        if (fd->is_class) { /* class var: a global variable that belongs to the type */
            class_fields(s, fd, t);
            continue;
        }
        if (s->kind == SK_RECORD) require_nilable(t, fd->ty->pos, "record field");
        else if (t->kind == T_PROC) require_nilable(t, fd->ty->pos, "field");
        int fborrowed = fd->borrowed;
        if (fborrowed && !is_owning_type(t)) {
            if (!generic_depth)
                error_at(fd->ty->pos, "a 'var' field (reference without ownership) needs a class type, not \"%s\"", type_name(t));
            fborrowed = 0; /* generic instance with a value type: the field holds the value */
        }
        if (t->readonly && t->kind != T_CLASS)
            error_at(fd->ty->pos, "fields cannot be 'const' (make the whole variable const: 'val x: T const := ...')");
        for (int k = 0; k < fd->names.len; k++) {
            Ident *id = &fd->names.items[k];
            for (int j = 0; j < s->fields.len; j++)
                if (STREQ(s->fields.items[j]->name, id->name))
                    error_at(id->pos, "duplicate field \"%s\"", id->name);
            Field *f = NEW(Field);
            f->name = id->name;
            f->cname = sfmt("f_%s", id->name);
            f->type = t;
            f->borrowed = fborrowed;
            f->owner = s;
            f->vis = fd->vis;
            vpush(s->fields, f);
        }
    }
}

static void build_params(Routine *rt, ParamGroupVec *pg);
static Routine *new_routine(const char *name, int kind, Pos p);

/* records: value types; methods are static (no inheritance, no virtual methods) */
/* enumeration with data: (Circle(Radius: Real), Rect(W, H: Real), Empty)
   = a record with a hidden tag and the fields of all variants ("Circle.Radius") */
static void define_sum(Struct *s, TypeExpr *te) {
    s->is_sum = 1;
    Field *tag = NEW(Field);
    tag->name = "#tag";
    tag->cname = "tag";
    tag->type = t_int;
    tag->owner = s;
    vpush(s->fields, tag);
    for (int i = 0; i < te->variants.len; i++) {
        VariantDecl *vd = &te->variants.items[i];
        for (int j = 0; j < i; j++)
            if (STREQ(te->variants.items[j].name.name, vd->name.name))
                error_at(vd->name.pos, "duplicate variant \"%s\"", vd->name.name);
        VariantInfo vi = {vd->name.name, s->fields.len, 0, vd->name.pos};
        for (int k = 0; k < vd->fields.len; k++) {
            FieldDecl *fd = &vd->fields.items[k];
            Type *t = resolve_type(fd->ty);
            no_closure(t, fd->ty->pos, "a variant field");
            check_complete(t, fd->ty->pos);
            require_nilable(t, fd->ty->pos, "variant field");
            if (t->readonly && t->kind != T_CLASS) error_at(fd->ty->pos, "fields cannot be 'const'");
            for (int n = 0; n < fd->names.len; n++) {
                Ident *id = &fd->names.items[n];
                for (int j = vi.first; j < s->fields.len; j++)
                    if (STREQ(s->fields.items[j]->name, sfmt("%s.%s", vd->name.name, id->name)))
                        error_at(id->pos, "duplicate field \"%s\" in variant \"%s\"", id->name, vd->name.name);
                Field *f = NEW(Field);
                f->name = sfmt("%s.%s", vd->name.name, id->name);
                f->cname = sfmt("f_%s_%s", vd->name.name, id->name);
                f->type = t;
                f->owner = s;
                vpush(s->fields, f);
                vi.n++;
            }
        }
        vpush(s->variants, vi);
    }
    /* the variant names construct values: Circle(2.0), Empty (generic instances: TOpt<Integer>.Some(5)) */
    if (!generic_depth && !archetype_mode)
        for (int i = 0; i < s->variants.len; i++) {
            Sym *v = add_sym(SY_VARIANT, s->variants.items[i].name, s->variants.items[i].pos);
            v->type = s->type;
            v->vidx = i;
        }
    s->defined = 1;
    if (!archetype_mode) vpush(prog->structs, s);
}

static void define_record(Struct *s, TypeExpr *te) {
    if (te->is_sum) {
        define_sum(s, te);
        return;
    }
    add_fields(s, &te->fields);
    StructDecl *sd = te->sd;
    if (sd && (sd->methods.len || sd->props.len) && strncmp(s->name, "record", 6) == 0 && isdigit((unsigned char)s->name[6]))
        error_at(te->pos, "records with methods must be declared as a named type");
    for (int i = 0; sd && i < sd->methods.len; i++) {
        MethodDecl *md = &sd->methods.items[i];
        if (md->tparams.len) { /* generic method: an instance for every type combination used */
            generic_method(s, md);
            continue;
        }
        if (md->is_operator) operator_rules(s, md);
        if (md->kind == RK_CTOR || md->kind == RK_DTOR)
            error_at(md->name.pos, "records cannot have constructors or destructors (records are values: use a procedure such as Init or a function that returns the record)");
        if (md->virt != VIRT_NONE || md->is_abstract)
            error_at(md->name.pos, "record methods cannot be virtual, abstract or override (records have no inheritance)");
        int novl = method_overload(s, md, "method");
        for (int j = 0; j < s->fields.len; j++)
            if (STREQ(s->fields.items[j]->name, md->name.name))
                error_at(md->name.pos, "\"%s\" is already a field of \"%s\"", md->name.name, s->name);
        Routine *r = new_routine(md->name.name, md->kind, md->name.pos);
        r->is_constm = md->is_const;
        r->is_classm = md->is_classm;
        classm_rules(md);
        if (md->is_const && (md->kind == RK_CTOR || md->kind == RK_DTOR))
            error_at(md->name.pos, "constructors and destructors cannot be const");
        r->owner = s;
        r->level = 1;
        r->cname = novl ? sfmt("M%d_%s_%d", s->id, md->name.name, novl + 1) : sfmt("M%d_%s", s->id, md->name.name);
        r->vis = md->vis;
        r->is_overload = md->is_overload;
        build_params(r, &md->params);
        if (md->ret) r->ret = resolve_type(md->ret), no_closure(r->ret, md->ret->pos, "the result of a method");
        r->ret_borrowed = md->ret_borrowed;
        auto_rules(md, s, r);
        overload_unique(s, r);
        if (md->is_operator) operator_check(s, r);
        vpush(s->methods, r);
    }
    for (int i = 0; sd && i < sd->props.len; i++) {
        PropDecl *pd = &sd->props.items[i];
        Prop *pr = NEW(Prop);
        pr->name = pd->name.name;
        pr->type = resolve_type(pd->ty);
        pr->read = pd->has_read ? pd->read.name : NULL;
        pr->write = pd->has_write ? pd->write.name : NULL;
        pr->is_default = pd->is_default;
        if (pd->idx.len) { /* indexer: the types of the index parameters */
            Routine tmp = {0};
            build_params(&tmp, &pd->idx);
            for (int k = 0; k < tmp.params.len; k++) vpush(pr->idx, tmp.params.items[k].type);
        }
        pr->pos = pd->name.pos;
        pr->vis = pd->vis;
        pr->owner = s;
        vpush(s->props, pr);
    }
    s->defined = 1;
    if (!archetype_mode) vpush(prog->structs, s); /* placeholder instances are only checked */
}

/* 'function Clone: T; const; auto;': the compiler writes a deep copy */
static void auto_rules(MethodDecl *md, Struct *s, Routine *r) {
    r->is_auto = md->is_auto;
    if (!md->is_auto) return;
    Pos p = md->name.pos;
    if (!STREQ(md->name.name, "Clone") || md->kind != RK_FUNC || md->params.len)
        error_at(p, "only 'function Clone: %s; const; auto;' can be generated by the compiler", s->name);
    if (!md->is_const) error_at(p, "a generated Clone only reads Self: declare it 'function Clone: %s; const; auto;'", s->name);
    if (md->is_classm || md->is_abstract) error_at(p, "a generated Clone cannot be a class method or abstract");
    Type *t = r->ret;
    if (md->ret_borrowed) error_at(p, "Clone returns a new object that the caller owns, not a 'var' reference");
    if (s->kind == SK_RECORD) {
        if (t->kind != T_RECORD || t->st != s) error_at(p, "Clone of record \"%s\" must return \"%s\"", s->name, s->name);
    } else if (t->kind != T_CLASS || t->nilable || t->readonly || !struct_descends(s, t->st))
        error_at(p, "Clone of \"%s\" must return \"%s\" or one of its ancestor classes", s->name, s->name);
    r->defined = 1; /* no implementation: the body is generated */
}

static void classm_rules(MethodDecl *md) {
    if (!md->is_classm) return;
    if (md->kind == RK_CTOR || md->kind == RK_DTOR)
        error_at(md->name.pos, "constructors and destructors cannot be class methods");
    if (md->virt != VIRT_NONE || md->is_abstract)
        error_at(md->name.pos, "class methods cannot be virtual, abstract or override");
    if (md->is_const) error_at(md->name.pos, "class methods have no Self: ';const;' is not allowed");
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
    error_at(te->pos, "invalid array index or set element type \"%s\" (an enumeration, Char, Boolean, Byte or a subrange)", type_name(t));
}

static Type *resolve_type_inner(TypeExpr *te);
static Type *resolve_type(TypeExpr *te) {
    Type *t = resolve_type_inner(te);
    if (te->readonly) t = readonly_any(t);
    if (!te->nilable) return t;
    no_closure(t, te->pos, "an optional value");
    if (is_optional(t)) error_at(te->pos, "optional of optional is not allowed");
    if (t->kind == T_CLASS) { /* optional class: a reference that may be none (NULL) */
        Type *n = NEW(Type);
        *n = *t;
        n->nilable = 1;
        n->name = sfmt("optional %s", type_name(t));
        return n;
    }
    if (t->kind == T_VOID) error_at(te->pos, "invalid optional type");
    Type *o = new_type(T_OPT, sfmt("optional %s", type_name(t))); /* value optional: flag + value */
    o->base = t;
    return o;
}

static void define_record(Struct *s, TypeExpr *te);
static void define_struct(Struct *s, StructDecl *sd);

/* bind the type parameters of g in a new scope at program level */
static Scope *enter_generic(Generic *g, Type **args, Scope **saved_scope, Routine **saved_cur, int *saved_level) {
    *saved_scope = scope;
    *saved_cur = cur;
    *saved_level = level;
    scope = g->scope ? g->scope : gscope;
    cur = NULL;
    level = 0;
    generic_depth++;
    vpush(gen_stack, cur_gen_id);
    cur_gen_id = 0;
    for (int i = 0; i < all_generics.len; i++)
        if (all_generics.items[i] == g) cur_gen_id = i + 1;
    Scope *s = push_scope();
    for (int i = 0; i < g->params.len; i++) add_sym(SY_TYPE, g->params.items[i].name, g->params.items[i].pos)->type = args[i];
    return s;
}
static void leave_generic(Scope *saved_scope, Routine *saved_cur, int saved_level) {
    generic_depth--;
    cur_gen_id = gen_stack.items[--gen_stack.len];
    scope = saved_scope;
    cur = saved_cur;
    level = saved_level;
}

/* name of an instance for messages and ClassName: TListe<TKunde> */
static char *inst_name(Generic *g, Type **args, int n) {
    Buf kb = {0};
    buf_printf(&kb, "%s<", g->name);
    for (int i = 0; i < n; i++) buf_printf(&kb, "%s%s", i ? ", " : "", type_name(args[i]));
    buf_put(&kb, ">");
    return xstrdup(buf_str(&kb));
}
/* cache key: like the name, but placeholders of different generics never collide */
static char *inst_key(Generic *g, Type **args, int n) {
    Buf kb = {0};
    buf_printf(&kb, "%s<", g->name);
    for (int i = 0; i < n; i++)
        if (args[i]->kind == T_PARAM) buf_printf(&kb, "%s%s#%p", i ? ", " : "", type_name(args[i]), (void *)args[i]);
        else buf_printf(&kb, "%s%s", i ? ", " : "", type_name(args[i]));
    buf_put(&kb, ">");
    return xstrdup(buf_str(&kb));
}

/* TListe<TKunde>: the instance of a generic type for these type arguments (made once) */
static Type *instantiate(Generic *g, Type **args, int n, Pos p) {
    for (int i = 0; i < n; i++) no_closure(args[i], p, "a type argument");
    if (n != g->params.len)
        error_at(p, "\"%s\" has %d type parameter%s, but %d type argument%s given", g->name, g->params.len,
                 g->params.len == 1 ? "" : "s", n, n == 1 ? " is" : "s are");
    check_constraints(g, args, p);
    char *key = inst_key(g, args, n);
    if (archetype_mode) {
        for (int i = 0; i < g->akeys.len; i++)
            if (STREQ(g->akeys.items[i], key)) return g->ainsts.items[i].st->type;
    } else
        for (int i = 0; i < g->keys.len; i++)
            if (STREQ(g->keys.items[i], key)) return g->insts.items[i].st->type;
    static int depth;
    if (++depth > 50) error_at(p, "generic instances nested too deeply (%s)", key);
    Scope *ss;
    Routine *sc;
    int sl;
    enter_generic(g, args, &ss, &sc, &sl);
    const char *save_ctx = g_err_context;
    char *shown = inst_name(g, args, n);
    g_err_context = shown;
    TypeExpr *te = reparse_type(g->toks, g->tok);
    int sk = te->kind == TE_RECORD ? SK_RECORD : te->sd->kind;
    Struct *st = new_struct(shown, sk, g->pos);
    Instance in = {st, (Type **)xcalloc(n ? n : 1, sizeof(Type *))};
    for (int i = 0; i < n; i++) in.args[i] = args[i];
    if (archetype_mode) {
        vpush(g->akeys, key);
        vpush(g->ainsts, in);
    } else {
        vpush(g->keys, key);
        vpush(g->insts, in); /* registered before it is defined: recursive uses (optional TKnoten<T>) find it */
    }
    if (te->kind == TE_RECORD) define_record(st, te);
    else define_struct(st, te->sd);
    g_err_context = save_ctx;
    leave_generic(ss, sc, sl);
    for (int i = 0; !archetype_mode && i < g->impls.len; i++) {
        PendingBody pb = {g, in, g->impls.items[i], NULL, NULL, NULL};
        vpush(pending_bodies, pb);
    }
    depth--;
    return st->type;
}

static Type *generic_type(Look *L, Ident name, TypeExprVec *targs, Pos p) {
    if (L->kind != LK_SYM || L->sym->kind != SY_GENERIC || L->sym->gen->is_routine)
        error_at(p, "\"%s\" is not a generic type", name.name);
    Type **args = (Type **)xcalloc(targs->len ? targs->len : 1, sizeof(Type *));
    for (int i = 0; i < targs->len; i++) args[i] = resolve_type(targs->items[i]);
    return instantiate(L->sym->gen, args, targs->len, p);
}

/* method bodies of generic instances: checked with the type arguments filled in */
static void routine_decl(RoutineDecl *d);
static void routine_body(Routine *rt, RoutineDecl *d);
static Sym *find_local(const char *name);

/* Max<Integer>: the instance of a generic routine (header now, body checked at the end) */
static Routine *instantiate_routine(Generic *g, Type **args, int n, Pos p) {
    for (int i = 0; i < n; i++) no_closure(args[i], p, "a type argument");
    if (n != g->params.len)
        error_at(p, "\"%s\" has %d type parameter%s, but %d type argument%s given", g->name, g->params.len,
                 g->params.len == 1 ? "" : "s", n, n == 1 ? " is" : "s are");
    check_constraints(g, args, p);
    char *key = inst_key(g, args, n);
    if (archetype_mode) {
        for (int i = 0; i < g->akeys.len; i++)
            if (STREQ(g->akeys.items[i], key)) return g->arts.items[i];
    } else
        for (int i = 0; i < g->keys.len; i++)
            if (STREQ(g->keys.items[i], key)) return g->rts.items[i];
    static int counter;
    Scope *ss;
    Routine *sc;
    int sl;
    enter_generic(g, args, &ss, &sc, &sl);
    const char *save_ctx = g_err_context;
    g_err_context = inst_name(g, args, n);
    Routine *rt;
    RoutineDecl *d;
    if (g->owner) { /* generic method: a method of its type, with the type arguments bound */
        if (!g->toks) gmethod_impl(g);
        d = reparse_routine(g->toks, g->tok);
        d->tparams = (IdentVec){0};
        MethodDecl *md = g->mdecl;
        rt = new_routine(g->name, md->kind, md->name.pos);
        rt->owner = g->owner;
        rt->level = 1;
        rt->cname = sfmt("G%d_%s", ++counter, g->name);
        rt->is_classm = md->is_classm;
        rt->is_constm = md->is_const;
        rt->vis = md->vis;
        rt->forward = 1;
        build_params(rt, &md->params);
        if (md->ret) rt->ret = resolve_type(md->ret);
        rt->ret_borrowed = md->ret_borrowed;
        if (rt->ret_borrowed && !is_owning_type(rt->ret)) {
            if (has_owned(rt->ret)) rt->ret_ref = 1;
            else rt->ret_borrowed = 0;
        }
        Routine tmp = {0};
        tmp.name = rt->name;
        build_params(&tmp, &d->params);
        if (!same_params(rt, &tmp) || d->kind != md->kind || d->is_classm != md->is_classm)
            error_at(d->pos, "the implementation of \"%s.%s\" does not match its declaration", g->owner->name, g->name);
        for (int i = 0; i < rt->params.len; i++) rt->params.items[i].name = tmp.params.items[i].name;
    } else {
        d = reparse_routine(g->toks, g->tok);
        d->tparams = (IdentVec){0};
        d->forward = 1; /* the header now: calls (also recursive ones) can be checked right away */
        inst_cname = sfmt("G%d_%s", ++counter, g->name);
        routine_decl(d);
        inst_cname = NULL;
        rt = find_local(d->name.name)->rt;
    }
    g_err_context = save_ctx;
    leave_generic(ss, sc, sl);
    Instance in = {NULL, (Type **)xcalloc(n ? n : 1, sizeof(Type *))};
    for (int i = 0; i < n; i++) in.args[i] = args[i];
    if (archetype_mode) {
        vpush(g->akeys, key);
        vpush(g->ainsts, in);
        vpush(g->arts, rt);
        if (g == self_check) { /* the generic's own placeholder instance: its body is checked once */
            g->arch_rd = d;
            g->arch_rt = rt;
        }
        return rt;
    }
    vpush(g->keys, key);
    vpush(g->insts, in);
    vpush(g->rts, rt);
    PendingBody pb = {g, in, NULL, rt, d, inst_name(g, args, n)};
    vpush(pending_bodies, pb);
    return rt;
}

static int tparam_index(Generic *g, const char *name) {
    for (int i = 0; i < g->params.len; i++)
        if (STREQ(g->params.items[i].name, name)) return i;
    return -1;
}

/* infer type arguments: match a parameter type (syntax) against the type of an argument */
static void unify(Generic *g, TypeExpr *te, Type *at, Type **b, Pos p) {
    if (!at || at->kind == T_LIT || at->kind == T_NIL || at->kind == T_VOID) return;
    if (te->kind == TE_NAMED && !te->targs.len) {
        int k = tparam_index(g, te->name.name);
        if (k < 0) return;
        Type *x = at;
        if (x->kind == T_CLASS && (x->readonly || x->nilable)) {
            if (te->nilable || x->readonly) x = x->st->type; /* optional T / T const: the class itself */
        }
        if (te->nilable && x->kind == T_OPT) x = x->base;
        if (x->kind == T_PTR && x->base && !STREQ(type_name(x), "")) { /* heap value: its value type */
            if (strncmp(type_name(x), "heap ", 5) == 0) x = x->base;
        }
        if (!b[k]) b[k] = x;
        else if (!same_type(b[k], x)) {
            if (b[k]->kind == T_INT && x->kind == T_REAL) b[k] = x; /* Max(3, 7.5): Real */
            else if (b[k]->kind == T_REAL && x->kind == T_INT) {
            } else if (b[k]->kind == T_INT && x->kind == T_INT) {
            } else if (b[k]->kind == T_CLASS && x->kind == T_CLASS && struct_descends(x->st, b[k]->st)) {
            } else if (b[k]->kind == T_CLASS && x->kind == T_CLASS && struct_descends(b[k]->st, x->st))
                b[k] = x;
            else
                error_at(p, "cannot infer '%s' of \"%s\": it is \"%s\" from one argument and \"%s\" from another; write '%s<...>(...)'",
                         g->params.items[k].name, g->name, type_name(b[k]), type_name(x), g->name);
        }
        return;
    }
    if (te->kind == TE_NAMED && te->targs.len && (at->kind == T_CLASS || at->kind == T_RECORD)) {
        Look L = lookup(te->name.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_GENERIC) return;
        Generic *tg = L.sym->gen;
        for (int i = 0; i < tg->insts.len; i++)
            if (tg->insts.items[i].st == at->st)
                for (int j = 0; j < te->targs.len && j < tg->params.len; j++) unify(g, te->targs.items[j], tg->insts.items[i].args[j], b, p);
        return;
    }
    if (te->kind == TE_ARRAY && (te->dyn ? at->kind == T_DYN : at->kind == T_ARRAY)) unify(g, te->elem, at->base, b, p);
    if (te->kind == TE_PROC && at->kind == T_PROC) { /* function(x, y: T): Boolean */
        RoutineDecl *d = te->proc;
        int k = 0;
        for (int i = 0; i < d->params.len; i++)
            for (int j = 0; j < d->params.items[i].names.len; j++, k++)
                if (k < at->proc->params.len) unify(g, d->params.items[i].ty, at->proc->params.items[k].type, b, p);
        if (d->ret && at->proc->ret) unify(g, d->ret, at->proc->ret, b, p);
    }
    if (te->kind == TE_RESULT && at->kind == T_RES) {
        if (te->elem && at->base) unify(g, te->elem, at->base, b, p);
        if (te->errt) unify(g, te->errt, at->err, b, p);
    }
}

static Expr *make_call(Routine *rt, Expr *base, ExprVec *args, int mode, Type *newcls, Pos p);
static Expr *value(Expr *e);
/* Max(3, 7): the type arguments come from the arguments */



/* ---- class references: class of TShape ---- */
static Type *classref_of(Struct *st) {
    Type *t = new_type(T_CLASSREF, sfmt("class of %s", st->name));
    t->st = st;
    return t;
}
/* c.Create(...), c.ClassName, c.SomeClassMethod(...) */
static Expr *classref_member(Expr *cref, const char *name, ExprVec *args, Pos p) {
    Struct *st = cref->type->st;
    if (STREQ(name, "ClassName")) {
        if (args && args->len) error_at(p, "ClassName takes no arguments");
        Expr *b = new_expr(E_BUILTIN, p);
        b->bi = BI_REFNAME;
        vpush(b->args, cref);
        b->type = t_str;
        return b;
    }
    Look M = {0};
    if (!find_member(st, name, &M.f, &M.m, &M.p) || !M.m || (M.m->kind != RK_CTOR && !M.m->is_classm))
        error_at(p, "through a class reference only constructors, class methods and ClassName can be used ('%s' is none)", name);
    check_visible(&M, p, name);
    if (M.m->is_classm) return make_call(M.m, NULL, args, CM_NORMAL, NULL, p); /* the class method of the declared class */
    Expr *c = make_call(M.m, NULL, args, CM_NEWCLASS, st->type, p); /* an object of the class the reference holds */
    c->b = cref;
    return c;
}
/* ---- class fields: class var Count: Integer ---- */
static Field *find_cfield(Struct *st, const char *name) {
    for (Struct *c = st; c; c = c->parent)
        for (int i = 0; i < c->cfields.len; i++)
            if (STREQ(c->cfields.items[i]->name, name)) return c->cfields.items[i];
    return NULL;
}
static void class_fields(Struct *s, FieldDecl *fd, Type *t) {
    require_nilable(t, fd->ty->pos, "class field");
    if (fd->borrowed) error_at(fd->ty->pos, "a class field cannot be a 'var' reference (it would outlive what it refers to)");
    for (int k = 0; k < fd->names.len; k++) {
        Ident *id = &fd->names.items[k];
        for (int j = 0; j < s->fields.len; j++)
            if (STREQ(s->fields.items[j]->name, id->name)) error_at(id->pos, "duplicate field \"%s\"", id->name);
        for (int j = 0; j < s->cfields.len; j++)
            if (STREQ(s->cfields.items[j]->name, id->name)) error_at(id->pos, "duplicate field \"%s\"", id->name);
        Var *v = new_var(id->name, t, VK_GLOBAL, id->pos);
        v->cname = sfmt("C%d_%s", s->id, id->name);
        v->is_global = 1;
        v->level = 0;
        if (!archetype_mode) vpush(prog->globals, v);
        Field *f = NEW(Field);
        f->name = id->name;
        f->cname = v->cname;
        f->type = t;
        f->owner = s;
        f->vis = fd->vis;
        f->cvar = v;
        vpush(s->cfields, f);
    }
}
static Expr *cfield_ref(Field *f, Pos p) {
    Look V = {0};
    V.f = f;
    check_visible(&V, p, f->name);
    return var_expr(f->cvar, p);
}
static Look cfield_look(Field *f) {
    Look V = {0};
    V.f = f;
    check_visible(&V, scope_pos, f->name);
    Sym *sy = NEW(Sym);
    sy->kind = SY_VAR;
    sy->name = f->name;
    sy->var = f->cvar;
    Look L = {0};
    L.kind = LK_SYM;
    L.sym = sy;
    return L;
}
/* ---- generic methods: function Id<T>(x: T): T in a class or record ---- */
static VEC(RoutineDecl *) gm_impls; /* their implementations (function TA.Id<T>...), known before they are checked */
static Generic *find_gmethod(Struct *st, const char *name) {
    for (Struct *c = st; c; c = c->parent)
        for (int i = 0; i < c->gmethods.len; i++)
            if (STREQ(((Generic *)c->gmethods.items[i])->name, name)) return c->gmethods.items[i];
    return NULL;
}
static void generic_method(Struct *s, MethodDecl *md) {
    if (s->is_iface) error_at(md->name.pos, "interfaces cannot have generic methods");
    if (md->virt != VIRT_NONE || md->is_abstract) error_at(md->name.pos, "generic methods cannot be virtual (an instance is made for each type)");
    if (md->kind == RK_CTOR || md->kind == RK_DTOR) error_at(md->name.pos, "constructors and destructors cannot be generic");
    if (generic_depth) error_at(md->name.pos, "generic methods of generic types are not supported (yet)");
    for (int j = 0; j < s->methods.len; j++)
        if (STREQ(s->methods.items[j]->name, md->name.name)) error_at(md->name.pos, "duplicate method \"%s\"", md->name.name);
    if (find_gmethod(s, md->name.name)) error_at(md->name.pos, "duplicate method \"%s\"", md->name.name);
    Generic *g = NEW(Generic);
    g->name = md->name.name;
    g->params = md->tparams;
    g->cons = md->tcons;
    g->pos = md->name.pos;
    g->is_routine = 1;
    g->owner = s;
    g->mdecl = md;
    g->scope = scope;
    RoutineDecl *rd = NEW(RoutineDecl); /* the declaration: parameter types for inference */
    rd->kind = md->kind;
    rd->name = md->name;
    rd->params = md->params;
    rd->has_params = 1;
    rd->ret = md->ret;
    g->rdecl = rd;
    validate_constraints(g);
    vpush(all_generics, g);
    vpush(s->gmethods, g);
}
static void gmethod_impl(Generic *g) {
    for (int i = 0; i < gm_impls.len; i++) {
        RoutineDecl *d = gm_impls.items[i];
        if (STREQ(d->cls.name, g->owner->name) && STREQ(d->name.name, g->name)) {
            g->toks = d->toks;
            g->tok = d->tok_start;
            return;
        }
    }
    error_at(g->pos, "the generic method \"%s.%s\" is not implemented ('function %s.%s<...>(...)')", g->owner->name, g->name,
             g->owner->name, g->name);
}
static Expr *gmethod_call(Generic *g, Expr *base, TypeExprVec *targs, ExprVec *args, Pos p) {
    Routine *r;
    if (targs && targs->len) {
        Type **ta = (Type **)xcalloc(targs->len, sizeof(Type *));
        for (int i = 0; i < targs->len; i++) ta[i] = resolve_type(targs->items[i]);
        r = instantiate_routine(g, ta, targs->len, p);
    } else
        r = infer_routine(g, args, p);
    if (r->vis != VIS_PUBLIC) {
        Look V = {0};
        V.m = r;
        check_visible(&V, p, g->name);
    }
    if (r->is_classm) base = NULL;
    else if (!base) error_at(p, "the method \"%s\" needs an instance", g->name);
    return make_call(r, base, args, r->is_classm ? CM_NORMAL : -1, NULL, p);
}
static Routine *infer_routine(Generic *g, ExprVec *args, Pos p) {
    RoutineDecl *d = g->rdecl;
    int np = 0;
    for (int i = 0; i < d->params.len; i++) np += d->params.items[i].names.len;
    int na = args ? args->len : 0;
    if (na != np) error_at(p, "wrong number of arguments calling \"%s\" (expected %d, got %d)", g->name, np, na);
    Type **b = (Type **)xcalloc(g->params.len ? g->params.len : 1, sizeof(Type *));
    int k = 0;
    for (int i = 0; i < d->params.len; i++)
        for (int j = 0; j < d->params.items[i].names.len; j++, k++) {
            Expr *a = value(args->items[k]);
            args->items[k] = a;
            unify(g, d->params.items[i].ty, a->type, b, a->pos);
        }
    for (int i = 0; i < g->params.len; i++)
        if (!b[i])
            error_at(p, "cannot infer type parameter '%s' of \"%s\" from the arguments: write '%s<...>(...)'",
                     g->params.items[i].name, g->name, g->name);
    return instantiate_routine(g, b, g->params.len, p);
}
static void run_pending_body(int k) {
    {
        PendingBody pb = pending_bodies.items[k];
        if (pb.g->broken) return; /* errors were already reported for the generic itself */
        Scope *ss;
        Routine *sc;
        int sl;
        if (pb.rt) { /* body of a generic routine instance */
            enter_generic(pb.g, pb.in.args, &ss, &sc, &sl);
            g_err_context = pb.key;
            pb.rt->forward = 0;
            routine_body(pb.rt, pb.rd);
            g_err_context = NULL;
            leave_generic(ss, sc, sl);
            return;
        }
        enter_generic(pb.g, pb.in.args, &ss, &sc, &sl);
        RoutineDecl *d = reparse_routine(pb.impl->toks, pb.impl->tok_start);
        inst_cls = pb.in.st;
        g_err_context = sfmt("%s.%s", pb.in.st->name, d->name.name);
        routine_decl(d);
        g_err_context = NULL;
        inst_cls = NULL;
        leave_generic(ss, sc, sl);
    }
}

static int find_member(Struct *st, const char *name, Field **f, Routine **m, Prop **p);

/* ---- constraints: T: class | Copy | Clone | Equatable | Comparable | TBasis ---- */
static const char *BUILTIN_CONS[] = {"class", "Copy", "Clone", "Equatable", "Comparable", "Numeric", "Hashable", NULL};
static int builtin_cons(const char *c) {
    for (int i = 0; BUILTIN_CONS[i]; i++)
        if (STREQ(BUILTIN_CONS[i], c)) return 1;
    return 0;
}
static void validate_constraints(Generic *g) {
    for (int i = 0; i < g->cons.len; i++)
        for (int k = 0; k < g->cons.items[i].len; k++) {
            Ident *c = &g->cons.items[i].items[k];
            if (builtin_cons(c->name)) continue;
            Look L = lookup(c->name);
            if (L.kind != LK_SYM || L.sym->kind != SY_TYPE || L.sym->type->kind != T_CLASS)
                error_at(c->pos, "unknown constraint '%s': use class, Copy, Clone, Equatable, Comparable, Numeric, Hashable or the name of a class",
                         c->name);
        }
}
static int has_clone(Type *t) {
    if (t->kind == T_CLASS || t->kind == T_RECORD) {
        Field *f;
        Routine *m;
        Prop *pr;
        return find_member(t->st, "Clone", &f, &m, &pr) && m && m->kind == RK_FUNC && !m->params.len;
    }
    if (t->kind == T_ARRAY || t->kind == T_DYN) return (is_ref_type(t->base) || has_owned(t->base)) ? has_clone(t->base) : 1;
    return 0;
}
static int satisfies(Type *t, const char *c) {
    if (t->kind == T_PARAM) { /* a placeholder can do what its own constraints allow */
        if (STREQ(c, "Copy")) return t->ik & PC_COPY;
        if (STREQ(c, "Clone")) return t->ik & PC_CLONE;
        if (STREQ(c, "Equatable")) return t->ik & (PC_EQ | PC_CMP | PC_NUM);
        if (STREQ(c, "Comparable")) return t->ik & (PC_CMP | PC_NUM);
        if (STREQ(c, "Numeric")) return t->ik & PC_NUM;
        if (STREQ(c, "Hashable")) return t->ik & (PC_HASH | PC_CMP | PC_NUM);
        return 0;
    }
    if (STREQ(c, "Numeric")) return t->kind == T_INT || t->kind == T_REAL;
    if (STREQ(c, "class")) return t->kind == T_CLASS;
    if (STREQ(c, "Copy")) return t->kind != T_CLASS && !is_ref_type(t) && !has_owned(t);
    if (STREQ(c, "Clone")) return has_clone(t);
    if (STREQ(c, "Equatable")) return is_ordinal(t) || t->kind == T_REAL || t->kind == T_STR || t->kind == T_CLASS || (is_aggregate(t) && equatable(t));
    if (STREQ(c, "Comparable")) return is_ordinal(t) || t->kind == T_REAL || t->kind == T_STR;
    if (STREQ(c, "Hashable")) return hashable(t);
    Look L = lookup(c); /* a class: the type argument must be it or a descendant */
    if (L.kind != LK_SYM || L.sym->kind != SY_TYPE) return 0;
    return t->kind == T_CLASS && struct_descends(t->st, L.sym->type->st);
}
static void check_constraints(Generic *g, Type **args, Pos p) {
    for (int i = 0; i < g->cons.len && i < g->params.len; i++)
        for (int k = 0; k < g->cons.items[i].len; k++) {
            const char *c = g->cons.items[i].items[k].name;
            if (satisfies(args[i], c)) continue;
            Buf b = {0};
            buf_printf(&b, "%s<", g->name);
            for (int j = 0; j < g->params.len; j++) buf_printf(&b, "%s%s", j ? ", " : "", g->params.items[j].name);
            buf_put(&b, ">");
            error_at(p, "\"%s\" does not satisfy '%s: %s' (required by %s)", type_name(args[i]), g->params.items[i].name, c,
                     buf_str(&b));
        }
}

static Type *resolve_type_inner(TypeExpr *te) {
    switch (te->kind) {
    case TE_NAMED: {
        Look L = lookup(te->name.name);
        if (te->targs.len) return generic_type(&L, te->name, &te->targs, te->pos);
        if (L.kind == LK_SYM && L.sym->kind == SY_GENERIC)
            error_at(te->pos, "\"%s\" is generic: give its type arguments, e.g. '%s<Integer>'", te->name.name, te->name.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE)
            error_at(te->pos, "type identifier expected, \"%s\" is %s", te->name.name,
                     L.kind == LK_NONE ? "unknown" : "not a type");
        return L.sym->type;
    }
    case TE_PTR: {
        Look L = lookup(te->name.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE)
            error_at(te->pos, "type identifier expected after '^' (\"%s\")", te->name.name);
        require_nilable(L.sym->type, te->pos, "pointer target");
        return ptr_to(L.sym->type);
    }
    case TE_STR: return t_str;
    case TE_PROC: { /* function(a, b: Integer): Integer */
        RoutineDecl *d = te->proc;
        Routine *sig = new_routine("function value", d->kind, te->pos);
        sig->is_sig = 1;
        sig->level = 1;
        sig->cname = sfmt("SIG%d", sig->id);
        build_params(sig, &d->params);
        if (d->kind == RK_FUNC) sig->ret = resolve_type(d->ret);
        if (sig->ret) no_closure(sig->ret, d->ret->pos, "the result of a function type");
        Type *pt = proc_type(sig);
        if (te->closure) closure_of(pt);
        return pt;
    }
    case TE_SET: { /* set of TColor / set of 'a'..'z' / set of Byte */
        Type *t = new_type(T_SET, NULL);
        t->hi = range_of(te->elem, &t->lo, &t->index);
        if (t->hi - t->lo >= 256)
            error_at(te->pos, "a set can have at most 256 values (%lld..%lld has %lld)", t->lo, t->hi, t->hi - t->lo + 1);
        t->base = t->index;
        t->name = t->base->kind == T_INT ? sfmt("set of %lld..%lld", t->lo, t->hi) : sfmt("set of %s", type_name(t->base));
        return t;
    }
    case TE_RESULT: {
        Type *b = te->elem ? resolve_type(te->elem) : NULL;
        Type *er = te->errt ? resolve_type(te->errt) : t_str;
        Type *parts[2] = {b, er};
        for (int i = 0; i < 2; i++) {
            Type *x = parts[i];
            if (!x) continue;
            if (i == 1 && (is_owning_type(x) || x->kind == T_CLASS || has_owned(x)))
                error_at(te->pos, "the error of a result must be a value without objects, not \"%s\"", type_name(x));
            if (x->kind == T_PTR) error_at(te->pos, "a result cannot hold \"%s\"", type_name(x));
            if (x->kind == T_RES) error_at(te->pos, "result of result is not allowed");
            no_closure(x, te->pos, "a result");
            if (x->kind == T_VOID) error_at(te->pos, "invalid result type");
        }
        if (b) check_complete(b, te->elem->pos);
        Type *r = new_type(T_RES, res_name(b, er));
        r->base = b;
        r->err = er;
        return r;
    }
    case TE_ARRAY: {
        Type *elem = resolve_type(te->elem);
        no_closure(elem, te->elem->pos, "an array element");
        check_complete(elem, te->elem->pos);
        require_nilable(elem, te->elem->pos, "array element");
        if (te->dyn) { /* array of T: owns a buffer on the heap that grows with SetLength */
            Type *d = new_type(T_DYN, sfmt("array of %s", type_name(elem)));
            d->base = elem;
            return d;
        }
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
        define_record(s, te);
        return s->type;
    }
    case TE_STRUCT:
    case TE_CLASSFWD: error_at(te->pos, "object and class types must be declared in a type section");
    case TE_CLASSREF: { /* class of TShape */
        Look L = lookup(te->name.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE || L.sym->type->kind != T_CLASS || L.sym->type->st->is_iface)
            error_at(te->name.pos, "'class of' needs a class, \"%s\" is not one", te->name.name);
        return classref_of(L.sym->type->st);
    }
    }
    return t_void;
}

/* ============================================================== routines */

static void build_params(Routine *rt, ParamGroupVec *pg) {
    rt->params.len = 0;
    for (int i = 0; i < pg->len; i++) {
        ParamGroup *g = &pg->items[i];
        Type *t = resolve_type(g->ty);
        int mode = g->mode;
        if (t->kind == T_PROC && t->closure && mode != PM_VALUE)
            error_at(g->ty->pos, "a 'reference to' parameter can only be a value parameter (a closure must not leave the call)");
        if (t->readonly && t->kind != T_CLASS && mode == PM_VALUE) mode = PM_CONSTREF; /* read-only, no copy */
        if (mode == PM_VALUE && has_owned(t)) mode = PM_CONSTREF; /* owns objects: borrowed read-only, not copied */
        if (g->mode == PM_OWNED && !is_owning_type(t) && !has_owned(t)) { /* a value that owns objects: it is moved in */
            if (!generic_depth)
                error_at(g->ty->pos, "'vao' parameters (taking ownership) need a class type or a value that owns objects, not \"%s\"",
                         type_name(t));
            mode = PM_VALUE; /* generic instance with a plain value: an ordinary copy */
        }
        for (int k = 0; k < g->names.len; k++) {
            Param p = {g->names.items[k].name, t, mode, NULL};
            vpush(rt->params, p);
        }
    }
}

/* overloading: the parameter types alone tell the variants apart */
static int same_params(Routine *a, Routine *b) {
    if (a->params.len != b->params.len) return 0;
    for (int i = 0; i < a->params.len; i++)
        if (!same_type(a->params.items[i].type, b->params.items[i].type)) return 0;
    return 1;
}
static char *sig_text(Routine *r) {
    Buf b = {0};
    buf_printf(&b, "%s(", r->name);
    for (int i = 0; i < r->params.len; i++) {
        Param *pm = &r->params.items[i];
        buf_printf(&b, "%s%s%s", i ? "; " : "", pm->mode == PM_VAR ? "var " : pm->mode == PM_OUT ? "out " : pm->mode == PM_OWNED ? "vao " : "",
                   type_name(pm->type));
    }
    buf_put(&b, ")");
    return xstrdup(buf_str(&b));
}
/* a new method md of s with the name of existing ones: allowed if all are marked 'overload' */
static int method_overload(Struct *s, MethodDecl *md, const char *what) {
    int n = 0;
    for (int j = 0; j < s->methods.len; j++)
        if (STREQ(s->methods.items[j]->name, md->name.name)) {
            if (!(md->is_overload || md->virt == VIRT_OVERRIDE) || !s->methods.items[j]->is_overload)
                error_at(md->name.pos, "duplicate %s \"%s\" (to give several %ss the same name, mark each of them with 'overload')", what,
                         md->name.name, what);
            n++;
        }
    return n;
}
static void overload_unique(Struct *s, Routine *r) {
    for (int j = 0; j < s->methods.len; j++) {
        Routine *o = s->methods.items[j];
        if (o != r && STREQ(o->name, r->name) && same_params(o, r))
            error_at(r->pos, "\"%s\" is overloaded twice with the same parameters", sig_text(r));
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
    r->gen_id = cur_gen_id;
    if (!archetype_mode) vpush(prog->routines, r);
    else vpush(prog->arch_routines, r);
    return r;
}

static int find_slot(Struct *s, const char *name) {
    for (int i = s->slots.len - 1; i >= 0; i--)
        if (STREQ(s->slots.items[i].name, name)) return i;
    return -1;
}

/* IShape = interface(IBase) function Area: Real; const; end */
static void define_iface(Struct *s, StructDecl *sd) {
    s->is_iface = 1;
    if (sd->has_parent) {
        Look L = lookup(sd->parent.name);
        Type *pt = sd->parent_targs.len ? generic_type(&L, sd->parent, &sd->parent_targs, sd->parent.pos)
                   : (L.kind == LK_SYM && L.sym->kind == SY_TYPE) ? L.sym->type : NULL;
        if (!pt || pt->kind != T_CLASS || !pt->st->is_iface)
            error_at(sd->parent.pos, "\"%s\" is not an interface (an interface can only extend an interface)", sd->parent.name);
        s->parent = pt->st;
    }
    if (prog->tobject) /* the C struct of an object reference: its VMT pointer */
        for (int i = 0; i < prog->tobject->fields.len; i++) vpush(s->fields, prog->tobject->fields.items[i]);
    for (int i = 0; i < sd->methods.len; i++) {
        MethodDecl *md = &sd->methods.items[i];
        if (md->tparams.len) { /* generic method: an instance for every type combination used */
            generic_method(s, md);
            continue;
        }
        if (md->is_operator) operator_rules(s, md);
        if (md->kind == RK_CTOR || md->kind == RK_DTOR)
            error_at(md->name.pos, "interfaces have no constructors or destructors (objects are created from classes)");
        if (md->virt != VIRT_NONE || md->is_abstract || md->is_classm || md->is_auto)
            error_at(md->name.pos, "interface methods are plain headers: no virtual, override, abstract, class or auto");
        Field *f;
        Routine *m;
        Prop *pr;
        for (int j = 0; j < s->methods.len; j++)
            if (STREQ(s->methods.items[j]->name, md->name.name))
                error_at(md->name.pos, "duplicate method \"%s\" (interfaces cannot overload methods)", md->name.name);
        if (md->is_overload) error_at(md->name.pos, "interfaces cannot overload methods");
        if (s->parent && find_member(s->parent, md->name.name, &f, &m, &pr) && m && m->owner->is_iface)
            error_at(md->name.pos, "\"%s\" is already declared in \"%s\"", md->name.name, m->owner->name);
        Routine *r = new_routine(md->name.name, md->kind, md->name.pos);
        r->is_constm = md->is_const;
        r->owner = s;
        r->level = 1;
        r->cname = sfmt("I%d_%s", s->id, md->name.name);
        build_params(r, &md->params);
        if (md->ret) r->ret = resolve_type(md->ret), no_closure(r->ret, md->ret->pos, "the result of a method");
        r->ret_borrowed = md->ret_borrowed;
        r->is_abstract = 1;
        r->defined = 1;
        r->slot = s->methods.len; /* position in the interface table */
        vpush(s->methods, r);
    }
    s->defined = 1;
    if (!archetype_mode) vpush(prog->structs, s);
}

/* the class c (or an ancestor) has a matching method for every method of interface i (and of what i extends) */
static void check_implements(Struct *c, Struct *i, Pos p) {
    for (; i; i = i->parent)
        for (int k = 0; k < i->methods.len; k++) {
            Routine *im = i->methods.items[k];
            Field *f;
            Routine *m;
            Prop *pr;
            if (!find_member(c, im->name, &f, &m, &pr) || !m || m->owner->is_iface)
                error_at(p, "\"%s\" does not implement the method \"%s\" of interface \"%s\"", c->name, im->name, i->name);
            if (m->is_overload) { /* overloaded: the variant with the interface's parameters */
                int found = 0;
                for (Struct *x = c; x && !found; x = x->parent)
                    for (int q = 0; q < x->methods.len && !found; q++)
                        if (STREQ(x->methods.items[q]->name, im->name) && same_params(x->methods.items[q], im)) {
                            m = x->methods.items[q];
                            found = 1;
                        }
            }
            int same_ret = (!im->ret && !m->ret) || (im->ret && m->ret && same_type(im->ret, m->ret));
            if (m->is_classm || !same_signature(im, m) || !same_ret || im->ret_borrowed != m->ret_borrowed)
                error_at(p, "\"%s.%s\" has a different signature than \"%s\" of interface \"%s\"", m->owner->name, im->name,
                         im->name, i->name);
            if (im->is_constm && !m->is_constm)
                error_at(p, "\"%s.%s\" must be declared 'const' like \"%s\" of interface \"%s\"", m->owner->name, im->name,
                         im->name, i->name);
        }
}

static void define_struct(Struct *s, StructDecl *sd) {
    if (sd->kind == SK_IFACE) {
        define_iface(s, sd);
        return;
    }
    Struct *par = NULL;
    if (sd->has_parent && sd->parent_targs.len) { /* class(TListe<TKunde>) */
        Look L = lookup(sd->parent.name);
        Type *pt = generic_type(&L, sd->parent, &sd->parent_targs, sd->parent.pos);
        if (pt->kind != T_CLASS) error_at(sd->parent.pos, "\"%s\" is not a class type", type_name(pt));
        par = pt->st;
    } else if (sd->has_parent) {
        Look L = lookup(sd->parent.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE || L.sym->type->kind != T_CLASS)
            error_at(sd->parent.pos, "\"%s\" is not a class type", sd->parent.name);
        par = L.sym->type->st;
        if (!par->defined) error_at(sd->parent.pos, "parent type \"%s\" is not completely defined", par->name);
    } else if (s->kind == SK_CLASS && prog->tobject && s != prog->tobject)
        par = prog->tobject;
    if (par && par->is_iface) { /* class(IShape): implements IShape, parent TObject */
        vpush(s->ifaces, par);
        par = prog->tobject;
    }
    for (int i = 0; i < sd->ifaces.len; i++) {
        TypeExpr *te = sd->ifaces.items[i];
        Type *it = resolve_type(te);
        if (it->kind != T_CLASS || !it->st->is_iface)
            error_at(te->pos, "\"%s\" is not an interface (a class has one parent class, then interfaces: class(TBase, IShape))",
                     type_name(it));
        for (int k = 0; k < s->ifaces.len; k++)
            if (s->ifaces.items[k] == it->st) error_at(te->pos, "interface \"%s\" is listed twice", it->st->name);
        vpush(s->ifaces, it->st);
    }
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
        if (md->tparams.len) { /* generic method: an instance for every type combination used */
            generic_method(s, md);
            continue;
        }
        if (md->is_operator) operator_rules(s, md);
        int novl = method_overload(s, md, md->kind == RK_CTOR ? "constructor" : "method");
        Routine *r = new_routine(md->name.name, md->kind, md->name.pos);
        r->is_overload = md->is_overload;
        r->is_constm = md->is_const;
        r->is_classm = md->is_classm;
        classm_rules(md);
        if (md->is_const && (md->kind == RK_CTOR || md->kind == RK_DTOR))
            error_at(md->name.pos, "constructors and destructors cannot be const");
        r->owner = s;
        r->level = 1;
        r->cname = novl ? sfmt("M%d_%s_%d", s->id, md->name.name, novl + 1) : sfmt("M%d_%s", s->id, md->name.name);
        r->vis = md->vis;
        build_params(r, &md->params);
        if (md->ret) r->ret = resolve_type(md->ret), no_closure(r->ret, md->ret->pos, "the result of a method");
        r->ret_borrowed = md->ret_borrowed;
        auto_rules(md, s, r);
        overload_unique(s, r);
        if (md->is_operator) operator_check(s, r);
        if (md->ret_borrowed && !is_owning_type(r->ret)) {
            if (has_owned(r->ret)) r->ret_ref = 1; /* a value that owns objects: a reference to it */
            else {
                if (!generic_depth) error_at(md->name.pos, "a 'var' result (reference without ownership) needs a class type or a value that owns objects");
                r->ret_borrowed = 0; /* generic instance with a plain value: the result is a copy */
            }
        }
        r->virt = md->virt;
        r->is_abstract = md->is_abstract;

        if (md->is_abstract && md->virt == VIRT_NONE)
            error_at(md->name.pos, "abstract methods must be virtual");
        if (md->virt == VIRT_OVERRIDE) {
            int idx = find_slot(s, md->name.name);
            for (int k = s->slots.len - 1; k >= 0; k--) /* overloads: the one with the same parameters */
                if (STREQ(s->slots.items[k].name, md->name.name) && same_params(s->slots.items[k].intro, r)) {
                    idx = k;
                    break;
                }
            if (idx < 0) error_at(md->name.pos, "there is no method in an ancestor class to be overridden: \"%s\"", md->name.name);
            if (s->slots.items[idx].intro->is_constm != r->is_constm)
                error_at(md->name.pos, "overriding method \"%s\" must %sbe declared 'const' like the method it overrides",
                         md->name.name, r->is_constm ? "not " : "");
            if (!same_signature(s->slots.items[idx].intro, r))
                error_at(md->name.pos, "overriding method \"%s\" has a different signature", md->name.name);
            s->slots.items[idx].impl = r;
            r->slot = idx;
            if (s->slots.items[idx].intro->is_overload) r->is_overload = 1; /* overrides one of several overloads */
        } else if (md->virt == VIRT_VIRTUAL) {
            Slot sl = {r->name, r, r};
            vpush(s->slots, sl);
            r->slot = s->slots.len - 1;
        }
        if (r->is_abstract) r->defined = 1;
        vpush(s->methods, r);
    }
    for (int i = 0; i < s->ifaces.len; i++) check_implements(s, s->ifaces.items[i], s->pos);
    for (int i = 0; i < sd->props.len; i++) {
        PropDecl *pd = &sd->props.items[i];
        Prop *p = NEW(Prop);
        p->name = pd->name.name;
        p->type = resolve_type(pd->ty);
        p->read = pd->has_read ? pd->read.name : NULL;
        p->write = pd->has_write ? pd->write.name : NULL;
        p->is_default = pd->is_default;
        if (pd->idx.len) { /* indexer: the types of the index parameters */
            Routine tmp = {0};
            build_params(&tmp, &pd->idx);
            for (int k = 0; k < tmp.params.len; k++) vpush(p->idx, tmp.params.items[k].type);
        }
        p->pos = pd->name.pos;
        p->vis = pd->vis;
        p->owner = s;
        vpush(s->props, p);
    }
    s->defined = 1;
    if (!archetype_mode) vpush(prog->structs, s); /* placeholder instances are only checked */
}

/* ============================================================== calls */

static Routine *enclosing_method(void) {
    for (Routine *r = cur; r; r = r->parent)
        if (r->owner) return r;
    return NULL;
}

static int var_compat(Type *formal, Type *actual) {
    if (formal->kind == T_CLASS && actual->kind == T_CLASS && actual->readonly && !formal->readonly) return 0;
    if (same_type(formal, actual)) return 1;
    if (formal->kind == T_CLASS && formal->kind == actual->kind &&
        struct_descends(actual->st, formal->st))
        return 1;
    return 0;
}

/* ---- overloading: the variant whose parameters fit the arguments best ---- */
static int lit_fits(Expr *a, Type *t) {
    switch (a->kind) {
    case E_LIST: return t->kind == T_ARRAY || t->kind == T_DYN || (t->kind == T_OPT && lit_fits(a, t->base));
    case E_RECLIT: return t->kind == T_RECORD || (t->kind == T_OPT && t->base->kind == T_RECORD);
    case E_OK: case E_ERR: return t->kind == T_RES;
    default: return 1; /* default fits every type */
    }
}
/* 0: the same type, 1: a widening (Integer to Int64, an object to its base class), 2: a conversion, -1: does not fit */
static int arg_cost(Param *pm, Expr *a) {
    Type *t = a->type;
    if (pm->mode == PM_VAR || pm->mode == PM_OUT) return is_lvalue(a) && var_compat(pm->type, t) ? 0 : -1;
    if (t->kind == T_LIT) return lit_fits(a, pm->type) ? 1 : -1;
    if (same_type(pm->type, t)) return 0;
    if (pm->type->kind == T_PARAM) return -1;
    int c = assign_compat(pm->type, t);
    if (!c && pm->type->kind == T_OPT && same_type(pm->type->base, t)) return 1; /* a value for an optional parameter */
    if (!c) return -1;
    return c == 1 ? 1 : 2;
}
static int in_overloads(Routine *first, Routine *r) {
    for (Routine *o = first; o; o = o->ovl_next)
        if (o == r) return 1;
    return 0;
}
static Routine *find_routine_sym(const char *name) {
    Sym *s = find_local(name);
    if (!s && iface_scope) s = find_in(iface_scope, name);
    return s && s->kind == SY_ROUTINE ? s->rt : NULL;
}
static Routine *pick_overload(Routine *rt, Expr *base, ExprVec *args, int mode, Type *newcls, Pos p) {
    VEC(Routine *) cands = {0};
    if (!rt->owner) {
        Routine *first = rt;
        Look L = lookup(rt->name);
        if (L.kind == LK_SYM && L.sym->kind == SY_ROUTINE && in_overloads(L.sym->rt, rt)) first = L.sym->rt;
        for (Routine *o = first; o; o = o->ovl_next) vpush(cands, o);
    } else { /* methods: this class and its ancestors, as long as they overload the name */
        Struct *from = rt->owner;
        if (mode != CM_INHERITED) {
            if (newcls && newcls->st) from = newcls->st;
            else if (base && base->type && (base->type->kind == T_CLASS || base->type->kind == T_RECORD)) from = base->type->st;
        }
        int stop = 0;
        for (Struct *c = from; c && !stop; c = c->parent)
            for (int i = 0; i < c->methods.len; i++) {
                Routine *m = c->methods.items[i];
                if (!STREQ(m->name, rt->name)) continue;
                if (!m->is_overload) stop = 1; /* a method without 'overload' hides the ancestors' ones */
                int dup = 0;
                for (int k = 0; k < cands.len; k++) dup |= same_params(cands.items[k], m); /* overridden */
                if (!dup) vpush(cands, m);
            }
    }
    if (cands.len == 1) return cands.items[0];
    for (int i = 0; i < args->len; i++) args->items[i] = value(args->items[i]);
    Routine *best = NULL;
    int bestc = 0, tie = 0;
    for (int k = 0; k < cands.len; k++) {
        Routine *c = cands.items[k];
        if (c->params.len != args->len) continue;
        int cost = 0;
        for (int i = 0; i < args->len && cost >= 0; i++) {
            int a = arg_cost(&c->params.items[i], args->items[i]);
            cost = a < 0 ? -1 : cost + a;
        }
        if (cost < 0) continue;
        if (!best || cost < bestc) best = c, bestc = cost, tie = 0;
        else if (cost == bestc) tie = 1;
    }
    Buf b = {0};
    for (int k = 0; k < cands.len; k++) buf_printf(&b, "%s%s", k ? ", " : "", sig_text(cands.items[k]));
    Buf at = {0};
    for (int i = 0; i < args->len; i++) buf_printf(&at, "%s%s", i ? ", " : "", type_name(args->items[i]->type));
    if (!best) error_at(p, "no overload of \"%s\" fits the arguments (%s); there are: %s", rt->name, buf_str(&at), buf_str(&b));
    if (tie) error_at(p, "the call of \"%s\" with (%s) is ambiguous; there are: %s", rt->name, buf_str(&at), buf_str(&b));
    return best;
}

static Expr *make_call(Routine *rt, Expr *base, ExprVec *args, int mode, Type *newcls, Pos p) {
    ExprVec empty = {0};
    if (!args) args = &empty;
    if (rt->is_overload) rt = pick_overload(rt, base, args, mode, newcls, p);
    if (args->len != rt->params.len)
        error_at(p, "wrong number of arguments calling \"%s\" (expected %d, got %d)", rt->name, rt->params.len,
                 args->len);
    if (rt->is_classm) { /* class method: no Self, the instance only names the class */
        base = NULL;
        mode = CM_NORMAL;
    }
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
        } else if (pm->type->kind == T_PROC) { /* Twice(p.Tick): a procedure without parameters as a value, not a call */
            Expr *r = ex(a);
            if (r->kind == E_TYPEREF) error_at(a->pos, "type \"%s\" used as a value", type_name(r->ctype));
            a = coerce(r, pm->type, sfmt("argument %d of \"%s\"", i + 1, rt->name));
        } else
            a = coerce(value(a), pm->type, sfmt("argument %d of \"%s\"", i + 1, rt->name));
        vpush(c->args, a);
    }
    if (mode < 0) {
        if (rt->owner) {
            if (rt->kind == RK_CTOR) mode = newcls ? CM_NEWCLASS : CM_CTOR_INST;
            else mode = rt->owner->is_iface ? CM_IFACE : rt->slot >= 0 ? CM_VIRTUAL : CM_STATIC;
        } else
            mode = CM_NORMAL;
    }
    if (rt->owner && !rt->is_classm && !base && mode != CM_NEWCLASS)
        error_at(p, "method \"%s\" needs an instance", rt->name);
    if (rt->owner && base && !rt->is_constm && mode != CM_NEWCLASS && !(rt->prelude && STREQ(rt->name, "Free")) &&
        (rooted_const(base) || (base->type->kind == T_CLASS && base->type->readonly) ||
         (base->kind == E_VAR && base->var->vkind == VK_SELF && base->var->is_const)))
        error_at(p, "cannot call '%s' on constant '%s': only const methods (declared with '; const;') are allowed",
                 rt->name, const_root_name(base));
    Routine *pr = pure_ctx();
    if (pr && !rt->is_constm && !rt->prelude) {
        int inside = 0;
        for (Routine *q = rt->parent; q; q = q->parent)
            if (q == pr) inside = 1;
        if ((!rt->owner || rt->is_classm) && !inside && rt != pr)
            error_at(p, "const %s '%s' can only call const routines, but '%s' is not const",
                     pr->kind == RK_FUNC ? "function" : "procedure", pr->name, rt->name);
        if (rt->owner && base && mode != CM_NEWCLASS && nonlocal_to_rt(root_var(base), pr))
            error_at(p, "const %s '%s' cannot call the non-const method '%s' on the non-local '%s'",
                     pr->kind == RK_FUNC ? "function" : "procedure", pr->name, rt->name, root_var(base)->name);
    }
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
        e->type->base->kind == T_RECORD) {
        Expr *d = new_expr(E_DEREF, e->pos);
        d->a = e;
        d->type = e->type->base;
        return d;
    }
    return e;
}

/* private members: only in the methods of their class; protected: also in its descendants */
static Routine *enclosing_method(void);
static void check_visible(Look *L, Pos p, const char *name) {
    int vis = L->f ? L->f->vis : L->m ? L->m->vis : L->p ? L->p->vis : VIS_PUBLIC;
    if (vis == VIS_PUBLIC) return;
    Struct *owner = L->f ? L->f->owner : L->m ? L->m->owner : L->p->owner;
    Routine *m = enclosing_method();
    Struct *ctx = m ? m->owner : NULL;
    if (ctx == owner) return;
    if (vis == VIS_PROTECTED && ctx && struct_descends(ctx, owner)) return;
    const char *what = L->f ? "field" : L->m ? (L->m->kind == RK_CTOR ? "constructor" : "method") : "property";
    error_at(p, "the %s \"%s\" of \"%s\" is %s%s", what, name, owner->name, vis == VIS_PRIVATE ? "private" : "protected",
             vis == VIS_PRIVATE ? " (only its own methods can use it)" : " (only its methods and those of its descendants can use it)");
}


/* ---- indexers: property Item[i: Integer]: T read Get write Put; default; ---- */
static Prop *default_prop(Struct *st) {
    for (Struct *c = st; c; c = c->parent)
        for (int i = 0; i < c->props.len; i++)
            if (c->props.items[i]->is_default) return c->props.items[i];
    return NULL;
}
/* e is x.Item[...], Item[...] (in a method) or x[...] with a default indexer: its object and property */
static int indexer_of(Expr *e, Expr **base, Prop **pr) {
    Expr *x = e->a;
    if (x->kind == E_MEMBER && !(x->a->kind == E_IDENT && lookup(x->a->id.name).kind == LK_SYM &&
                                 lookup(x->a->id.name).sym->kind == SY_TYPE)) {
        Expr *b = autoderef(value(x->a));
        x->a = b;
        if (b->type->kind == T_CLASS || b->type->kind == T_RECORD) {
            Look L = {0};
            if (find_member(b->type->st, x->id.name, &L.f, &L.m, &L.p) && L.p && L.p->idx.len) {
                check_visible(&L, x->pos, x->id.name);
                *base = b;
                *pr = L.p;
                return 1;
            }
        }
        return 0;
    }
    if (x->kind == E_IDENT && !x->targs.len) {
        Look L = lookup(x->id.name);
        if (L.kind == LK_MEMBER && L.p && L.p->idx.len) {
            *base = var_expr(L.base, x->pos);
            *pr = L.p;
            return 1;
        }
        if (L.kind == LK_MEMBER || L.kind == LK_NONE) return 0;
    }
    Expr *a = value(x);
    e->a = a;
    Type *t = a->type;
    if ((t->kind == T_CLASS || t->kind == T_RECORD) && default_prop(t->st)) {
        *base = a;
        *pr = default_prop(t->st);
        return 1;
    }
    return 0;
}
static Routine *indexer_accessor(Expr *base, Prop *pr, int write, Pos p) {
    const char *nm = write ? pr->write : pr->read;
    if (!nm) error_at(p, "the indexer \"%s\" is %s", pr->name, write ? "read-only (it has no 'write')" : "write-only (it has no 'read')");
    Look R = {0};
    if (!find_member(base->type->st, nm, &R.f, &R.m, &R.p) || !R.m)
        error_at(pr->pos, "the %s \"%s\" of the indexer \"%s\" must be a method", write ? "writer" : "reader", nm, pr->name);
    Routine *m = R.m;
    int n = pr->idx.len;
    int ok = m->params.len == n + write && (write ? m->kind == RK_PROC : m->kind == RK_FUNC);
    for (int i = 0; ok && i < n; i++) ok = same_type(m->params.items[i].type, pr->idx.items[i]);
    if (ok && !write) ok = same_type(m->ret, pr->type);
    if (ok && write) { /* the value: the property's type, or for an optional property the value inside */
        Type *vt = m->params.items[n].type, *pt = pr->type;
        ok = same_type(vt, pt) || (pt->kind == T_OPT && same_type(vt, pt->base)) ||
             (pt->kind == T_CLASS && pt->nilable && vt->kind == T_CLASS && vt->st == pt->st && !vt->nilable);
    }
    if (!ok)
        error_at(pr->pos, "the %s \"%s\" of the indexer \"%s\" must be a %s with the index parameters%s",
                 write ? "writer" : "reader", nm, pr->name, write ? "procedure" : "function",
                 write ? " and then the value" : sfmt(" and the result type \"%s\"", type_name(pr->type)));
    return m;
}
static Expr *member_ref(Expr *base, Look *L, ExprVec *args, Pos p, const char *name) {
    if (L->f) {
        if (args && L->f->type->kind == T_PROC) {
            Expr *fe = new_expr(E_FIELD, p);
            fe->a = base;
            fe->field = L->f;
            fe->type = L->f->type;
            return indirect_call(fe, args, p);
        }
        if (args) error_at(p, "\"%s\" is a field, not a method", name);
        Expr *e = new_expr(E_FIELD, p);
        e->a = base;
        e->field = L->f;
        e->type = L->f->type;
        if (e->type->kind == T_CLASS && ((base->type->kind == T_CLASS && base->type->readonly) || rooted_const(base)))
            e->type = readonly_of(e->type);
        return e;
    }
    if (L->m && !args && L->m->params.len && base && !L->m->is_classm && L->m->kind != RK_CTOR && L->m->kind != RK_DTOR) {
        int mode = L->m->owner->is_iface ? CM_IFACE : L->m->slot >= 0 ? CM_VIRTUAL : CM_STATIC;
        return method_value(L->m, base, mode, p); /* obj.Print without arguments: a method value */
    }
    if (L->m) return make_call(L->m, base, args, -1, NULL, p);
    if (args) error_at(p, "\"%s\" is a property, not a method", name);
    Prop *pr = L->p;
    if (pr->idx.len) error_at(p, "\"%s\" is an indexed property: write '%s[...]'", name, name);
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
    else if (t->kind == T_RECORD && f->kind == t->kind && is_lvalue(a)) ok = 1;
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
        scope_pos = p;
        Look L = lookup(name);
        if (L.kind == LK_NONE) {
            int bi = builtin_id(name);
            if (bi) return builtin(bi, args, p);
            for (int i = 0; i < broken_names.len; i++)
                if (STREQ(broken_names.items[i], name) && g_recover) longjmp(*g_recover, 1); /* already reported */
            if (lambda_outer) { /* a local of the routine around a lambda? */
                Scope *sv = scope;
                scope = lambda_outer;
                Look O = lookup(name);
                scope = sv;
                if (O.kind == LK_SYM && O.sym->kind == SY_VAR && !O.sym->var->is_global)
                    error_at(p, "a lambda cannot use the local variable '%s' of the routine around it (pass it as a parameter)", name);
                if (O.kind == LK_MEMBER || (O.kind == LK_SYM && O.sym->kind == SY_VAR && O.sym->var->vkind == VK_SELF))
                    error_at(p, "a lambda cannot use '%s' of the object around it (pass the object as a parameter)", name);
            }
            error_at(p, "identifier not found \"%s\"", name);
        }
        if (L.kind == LK_MEMBER && L.gm) /* Id(5) / Id<Integer>(5) inside a method: a generic method of Self */
            return gmethod_call(L.gm, var_expr(L.base, p), &e->targs, args, p);
        if (L.kind == LK_MEMBER) {
            check_visible(&L, p, name);
            return member_ref(L.base ? var_expr(L.base, p) : NULL, &L, args, p, name);
        }
        if (e->targs.len && L.kind == LK_SYM && L.sym->kind == SY_GENERIC && L.sym->gen->is_routine) { /* Max<Real>(...) */
            Type **ta = (Type **)xcalloc(e->targs.len, sizeof(Type *));
            for (int i = 0; i < e->targs.len; i++) ta[i] = resolve_type(e->targs.items[i]);
            return make_call(instantiate_routine(L.sym->gen, ta, e->targs.len, p), NULL, args, -1, NULL, p);
        }
        if (e->targs.len) { /* TListe<TKunde> as a type: cast or type reference */
            Type *gt = generic_type(&L, e->id, &e->targs, p);
            if (args) return cast_expr(gt, args, p);
            Expr *t = new_expr(E_TYPEREF, p);
            t->ctype = gt;
            t->type = gt;
            return t;
        }
        Sym *s = L.sym;
        switch (s->kind) {
        case SY_GENERIC:
            if (s->gen->is_routine) return make_call(infer_routine(s->gen, args, p), NULL, args, -1, NULL, p);
            error_at(p, "\"%s\" is generic: give its type arguments, e.g. '%s<Integer>'", name, name);
        case SY_VAR:
            if (args && s->var->type->kind == T_PROC) return indirect_call(var_expr(s->var, p), args, p);
            if (args) error_at(p, "\"%s\" is a variable and cannot be called", name);
            if (s->var->pending)
                error_at(p, "the type of '%s' is not known yet: assign a value to it first ('%s := ...')", name, name);
            if (s->var->is_box) { /* heap value: used like the value it holds */
                Expr *d = new_expr(E_DEREF, p);
                d->a = var_expr(s->var, p);
                d->type = s->var->type->base;
                return d;
            }
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
        case SY_VARIANT: return variant_value(s->type, s->vidx, args, p);
        case SY_ROUTINE:
            if (s->rt->ovl_next) { /* overloaded: 'F' is a call of the variant without parameters */
                if (!args) {
                    for (Routine *o = s->rt; o; o = o->ovl_next)
                        if (!o->params.len) return make_call(o, NULL, NULL, -1, NULL, p);
                    error_at(p, "\"%s\" is overloaded and cannot be used as a value (wrap the call in a lambda)", name);
                }
                return make_call(s->rt, NULL, args, -1, NULL, p);
            }
            if (!args && s->rt->params.len) return routine_ref(s->rt, p); /* used as a value */
            return make_call(s->rt, NULL, args, -1, NULL, p);
        }
    }
    if (e->kind == E_MEMBER) {
        const char *name = e->id.name;
        if (e->a->kind == E_IDENT) {
            Look L = lookup(e->a->id.name);
            Sym gsym = {0};
            if (e->a->targs.len) { /* TListe<TKunde>.Create(...) */
                gsym.kind = SY_TYPE;
                gsym.type = generic_type(&L, e->a->id, &e->a->targs, e->a->pos);
                L.kind = LK_SYM;
                L.sym = &gsym;
            }
            if (L.kind == LK_SYM && L.sym->kind == SY_VAR && L.sym->var->is_box && STREQ(name, "Free")) {
                if (args && args->len) error_at(p, "Free takes no arguments");
                Expr *f = new_expr(E_BUILTIN, p);
                f->bi = BI_BOXFREE;
                f->type = t_void;
                vpush(f->args, var_expr(L.sym->var, p));
                return f;
            }
            if (L.kind == LK_SYM && L.sym->kind == SY_TYPE) {
                Type *t = L.sym->type;
                if (t->kind == T_RECORD && t->st->is_sum) { /* TShape.Circle(2.0) */
                    for (int i = 0; i < t->st->variants.len; i++)
                        if (STREQ(t->st->variants.items[i].name, name)) return variant_value(t, i, args, p);
                    error_at(p, "\"%s\" has no variant \"%s\" (%s)", type_name(t), name, variant_list(t->st));
                }
                if ((t->kind == T_CLASS || t->kind == T_RECORD) && find_cfield(t->st, name)) { /* TCounter.Count */
                    if (args) error_at(p, "\"%s\" is a class field, not a method", name);
                    return cfield_ref(find_cfield(t->st, name), p);
                }
                if ((t->kind == T_CLASS || t->kind == T_RECORD) && find_gmethod(t->st, name)) /* TFactory.Make<TItem>() */
                    return gmethod_call(find_gmethod(t->st, name), NULL, &e->targs, args, p);
                if (t->kind == T_CLASS || t->kind == T_RECORD) {
                    Look M = {0};
                    if (find_member(t->st, name, &M.f, &M.m, &M.p) && M.m && M.m->is_classm) {
                        check_visible(&M, p, name);
                        return make_call(M.m, NULL, args, CM_NORMAL, NULL, p);
                    }
                }
                if (t->kind == T_CLASS && t->st->is_iface)
                    error_at(p, "\"%s\" is an interface: create an object of a class that implements it", t->st->name);
                if (t->kind == T_CLASS) {
                    Look M = {0};
                    if (!find_member(t->st, name, &M.f, &M.m, &M.p) || !M.m || M.m->kind != RK_CTOR)
                        error_at(p, "\"%s.%s\" is neither a constructor nor a class method", t->st->name, name);
                    check_visible(&M, p, name);
                    return make_call(M.m, NULL, args, CM_NEWCLASS, t, p);
                }
                if (STREQ(name, "Create")) { /* T.Create(init): allocate a heap value */
                    Expr *bn = builtin(BI_BOXNEW, args, p);
                    bn->ctype = t;
                    if (bn->args.len) bn->args.items[0] = coerce(bn->args.items[0], t, "Create");
                    bn->type = ptr_to(t);
                    bn->type->name = sfmt("heap %s", type_name(t));
                    return bn;
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
        if (bt->kind == T_CLASSREF) return classref_member(base, name, args, p); /* c.Create(...) */
        if (bt->kind == T_RES) {
            ExprVec a2 = {0};
            vpush(a2, base);
            if (args)
                for (int i = 0; i < args->len; i++) vpush(a2, args->items[i]);
            if (STREQ(name, "IsOk")) return builtin(BI_ISOK, &a2, p);
            if (STREQ(name, "IsError")) return builtin(BI_ISERROR, &a2, p);
            if (STREQ(name, "ValueOr")) return builtin(BI_VALUEOR, &a2, p);
            error_at(p, "'%s' is a result (%s): unpack it first, e.g. 'if r is ok v then ...' (members: IsOk, IsError, ValueOr)",
                     base->kind == E_VAR ? base->var->name : "x", type_name(bt));
        }
        if (is_optional(bt)) {
            if (STREQ(name, "HasValue") || STREQ(name, "ValueOr")) {
                ExprVec a2 = {0};
                vpush(a2, base);
                if (args)
                    for (int i = 0; i < args->len; i++) vpush(a2, args->items[i]);
                return builtin(STREQ(name, "HasValue") ? BI_HASVALUE : BI_VALUEOR, &a2, p);
            }
            if (!(bt->kind == T_CLASS && STREQ(name, "Free"))) {
                const char *what = base->kind == E_VAR ? base->var->name : base->kind == E_FIELD ? base->field->name : "x";
                error_at(p, "'%s' is optional (%s): unpack it first, e.g. 'if %s is some v then v.%s'", what,
                         type_name(bt), what, name);
            }
        }
        if (bt->kind == T_PARAM) {
            if (STREQ(name, "Clone") && (bt->ik & PC_CLONE) && !(args && args->len)) {
                Expr *c = new_expr(E_BUILTIN, p); /* only checked, never generated */
                c->bi = BI_ARRCLONE;
                vpush(c->args, base);
                c->type = bt;
                return c;
            }
            error_at(p, "'%s' is not known for type parameter \"%s\": add a constraint that provides it (e.g. '%s: TBasis' for a class, '%s: Clone' for Clone)",
                     name, type_name(bt), type_name(bt), type_name(bt));
        }
        if ((bt->kind == T_ARRAY || bt->kind == T_DYN) && STREQ(name, "Clone")) { /* a.Clone: copy, cloning the objects it owns */
            if (args && args->len) error_at(p, "Clone takes no arguments");
            Expr *c = new_expr(E_BUILTIN, p);
            c->bi = BI_ARRCLONE;
            vpush(c->args, base);
            Type *t = bt;
            if (t->readonly) { /* the copy can be changed */
                t = NEW(Type);
                *t = *bt;
                t->readonly = 0;
                t->name = NULL;
            }
            c->type = t;
            return c;
        }
        if (bt->kind != T_RECORD && bt->kind != T_CLASS)
            error_at(p, "illegal qualifier: \"%s\" has no members", type_name(bt));
        Look L = {0};
        if (bt->kind == T_CLASS && !bt->st->is_iface && STREQ(name, "ClassType") && !find_member(bt->st, name, &L.f, &L.m, &L.p)) {
            if (args && args->len) error_at(p, "ClassType takes no arguments");
            Expr *b = new_expr(E_BUILTIN, p); /* obj.ClassType: the class of the object */
            b->bi = BI_CLASSTYPE;
            vpush(b->args, base);
            b->type = classref_of(bt->st);
            return b;
        }
        if (!find_member(bt->st, name, &L.f, &L.m, &L.p) && find_cfield(bt->st, name)) {
            if (args) error_at(p, "\"%s\" is a class field, not a method", name);
            return cfield_ref(find_cfield(bt->st, name), p);
        }
        if (!find_member(bt->st, name, &L.f, &L.m, &L.p) && find_gmethod(bt->st, name))
            return gmethod_call(find_gmethod(bt->st, name), base, &e->targs, args, p);
        if (!find_member(bt->st, name, &L.f, &L.m, &L.p))
{
            if (bt->st->is_sum)
                error_at(p, "\"%s\" is an enumeration with data: its fields are read with 'case' or 'if x is Variant(a, b)'", bt->st->name);
            error_at(p, "identifier \"%s\" is not a member of \"%s\"", name, bt->st->name);
            }
        check_visible(&L, p, name);
        return member_ref(base, &L, args, p, name);
    }
    if (args) {
        Expr *f = value(e);
        if (f->type->kind == T_PROC) return indirect_call(f, args, p);
        error_at(p, "expression is not callable");
    }
    return value(e);
}

static Expr *inherited_call(Expr *e) {
    Routine *m = enclosing_method();
    if (!m) error_at(e->pos, "'inherited' is only allowed inside methods");
    if (m->is_classm) error_at(e->pos, "'inherited' is not available in class methods");
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
        if (e == forin_call) {
            Expr *r = resolve_ref(e->a, &e->args);
            r->forin = 1;
            return r;
        }
        if (e->a->kind == E_IDENT || e->a->kind == E_MEMBER) return resolve_ref(e->a, &e->args);
        if (e->a->kind == E_TYPEREF) return cast_expr(e->a->ctype, &e->args, e->pos); /* made by the compiler: T(x) */
        {
            Expr *f = value(e->a); /* Pick(True)(1, 5): call of a function value */
            if (f->type->kind == T_PROC) return indirect_call(f, &e->args, e->pos);
        }
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
        {
            Expr *ib = NULL;
            Prop *ip = NULL;
            if (indexer_of(e, &ib, &ip)) { /* l[i]: the reader of the indexer */
                if (e->args.len != ip->idx.len)
                    error_at(e->pos, "the indexer \"%s\" needs %d index%s", ip->name, ip->idx.len, ip->idx.len == 1 ? "" : "es");
                return make_call(indexer_accessor(ib, ip, 0, e->pos), ib, &e->args, -1, NULL, e->pos);
            }
        }
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
            if (a->type->kind == T_DYN) { /* index from 0, checked at run time */
                if (!is_int(idx->type)) error_at(idx->pos, "integer index expected for \"%s\"", type_name(a->type));
                n->type = a->type->base;
                if (rooted_const(a)) n->type = readonly_of(n->type);
            } else if (a->type->kind == T_ARRAY) {
                Type *it = a->type->index;
                if (!is_ordinal(idx->type) || (is_int(it) ? !is_int(idx->type) : !same_type(it, idx->type)))
                    error_at(idx->pos, "incompatible index type: got \"%s\" expected \"%s\"", type_name(idx->type),
                             type_name(it));
                n->type = a->type->base;
                if (rooted_const(a)) n->type = readonly_of(n->type);
                long long cv;
                Expr *c = fold(idx);
                if (c && lit_ord(c, &cv)) {
                    if (cv < a->type->lo || cv > a->type->hi)
                        error_at(idx->pos, "index %lld is outside the array bounds %lld..%lld", cv, a->type->lo, a->type->hi);
                    n->in_range = 1; /* checked: no runtime range check needed */
                }
            } else if (a->type->kind == T_STR) {
                if (!is_int(idx->type)) error_at(idx->pos, "integer index expected");
                long long cv;
                Expr *c = fold(idx);
                if (c && lit_ord(c, &cv)) {
                    if (cv < 1) error_at(idx->pos, "string index %lld: strings are indexed from 1", cv);
                    n->in_range = 1;
                }
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
        if (t->kind == T_RECORD) { /* -v: class operator Negative */
            Expr *r = user_operator(e, e->op, e->a, NULL);
            if (r) return r;
        }
        if (e->op == OP_NOT) {
            if (t->kind == T_BOOL) e->type = t_bool;
            else if (is_int(t)) e->type = arith_int(t, t);
            else error_at(e->pos, "'not' needs a Boolean or integer operand");
        } else if (t->kind == T_PARAM) {
            if (!(t->ik & PC_NUM)) error_at(e->pos, "'-' on \"%s\" needs the constraint '%s: Numeric'", type_name(t), type_name(t));
            e->type = t;
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
        if (ta->kind == T_PARAM || tb->kind == T_PARAM) return param_binary(e);
        if (ta->kind == T_SET || tb->kind == T_SET || (e->a->kind == E_SET && e->b->kind == E_SET)) return set_binary(e);
        if (ta->kind == T_RECORD || tb->kind == T_RECORD) { /* class operator Add(a, b: TVec): TVec ... */
            Expr *r = user_operator(e, op, e->a, e->b);
            if (r) return r;
            if (op == OP_NE && (has_operator(ta, "Equal") || has_operator(tb, "Equal"))) { /* a <> b: not Equal(a, b) */
                Expr *n = new_expr(E_UNARY, e->pos);
                n->op = OP_NOT;
                n->a = user_operator(e, OP_EQ, e->a, e->b);
                n->type = t_bool;
                return n;
            }
            if (op != OP_EQ && op != OP_NE && op != OP_AND && op != OP_OR)
                error_at(e->pos, "\"%s\" has no operator for this ('class operator %s(...)')", type_name(ta->kind == T_RECORD ? ta : tb),
                         op == OP_ADD ? "Add" : op == OP_SUB ? "Subtract" : op == OP_MUL ? "Multiply" : op == OP_DIV ? "Divide" :
                         op == OP_LT ? "LessThan" : op == OP_GT ? "GreaterThan" : op == OP_LE ? "LessThanOrEqual" :
                         op == OP_GE ? "GreaterThanOrEqual" : op == OP_IDIV ? "IntDivide" : op == OP_MOD ? "Modulus" : "...");
        }
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
            if ((ta->kind == T_NIL || tb->kind == T_NIL) && (op == OP_EQ || op == OP_NE)) {
                Type *o = ta->kind == T_NIL ? tb : ta;
                if (is_optional(o) || o->kind == T_NIL) return e;
                error_at(e->pos, "\"%s\" is not optional and can never be none", type_name(o));
            }
            if ((op == OP_EQ || op == OP_NE) && ta->kind == T_OPT && tb->kind == T_OPT && same_type(ta, tb) && equatable(ta))
                return e; /* both empty, or both with equal values */
            if (is_optional(ta) || is_optional(tb))
                error_at(e->pos, "optional values can only be compared with none or with an optional value of the same type; unpack them with 'is some'");
            if (is_ptrlike(ta) && is_ptrlike(tb) && (op == OP_EQ || op == OP_NE)) {
                if (assign_compat(ta, tb) || assign_compat(tb, ta)) return e;
            }
            if ((op == OP_EQ || op == OP_NE) && (is_aggregate(ta) || is_aggregate(tb))) { /* records, arrays: field by field */
                if (ta->kind == T_LIT) e->a = coerce(e->a, tb, "comparison"), ta = tb;
                if (tb->kind == T_LIT) e->b = coerce(e->b, ta, "comparison"), tb = ta;
                if (!same_agg(ta, tb))
                    error_at(e->pos, "incompatible types in comparison: \"%s\" and \"%s\"", type_name(ta), type_name(tb));
                if (!equatable(ta))
                    error_at(e->pos, "\"%s\" cannot be compared with '=': it contains a function value or a result", type_name(ta));
                return e;
            }
            error_at(e->pos, "incompatible types in comparison: \"%s\" and \"%s\"", type_name(ta), type_name(tb));
        }
    }
    case E_IN: {
        e->a = value(e->a);
        if (!is_ordinal(e->a->type)) error_at(e->pos, "ordinal expression expected before 'in'");
        if (e->b->kind != E_SET) { /* x in s */
            e->b = value(e->b);
            Type *st = e->b->type;
            if (st->kind != T_SET) error_at(e->b->pos, "a set or [...] expected after 'in', not \"%s\"", type_name(st));
            if (!set_elem_ok(st, e->a->type)) error_at(e->a->pos, "\"%s\" is not an element type of \"%s\"", type_name(e->a->type), type_name(st));
            e->type = t_bool;
            return e;
        }
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
    case E_ISSOME:
        error_at(e->pos, "'is %s' is only allowed in if/while conditions (alone or combined with 'and')",
                 e->op == 0 ? "some" : e->op == 1 ? "ok" : "error");
    case E_SOMEPAT:
        error_at(e->pos, "'%s' is only allowed as a case label for an %s selector",
                 e->op == 0 ? "some x" : e->op == 1 ? "ok v" : "error e", e->op == 0 ? "optional" : "result");
    case E_IS: case E_AS: {
        e->a = value(e->a);
        if (e->kind == E_IS && e->a->type->kind == T_RECORD && e->a->type->st->is_sum) return variant_test(e);
        if (e->kind == E_IS && e->has_args) error_at(e->pos, "'is Name(...)' tests a variant of an enumeration with data");
        if (e->a->type->kind != T_CLASS) error_at(e->pos, "class instance expected before 'is'/'as'");
        Look L = lookup(e->id.name);
        if (L.kind == LK_SYM && L.sym->kind == SY_VAR && L.sym->var->type->kind == T_CLASSREF) { /* obj is c: the class in c */
            e->b = var_expr(L.sym->var, e->pos);
            e->ctype = L.sym->var->type->st->type;
            e->type = e->kind == E_IS ? t_bool : e->a->type->readonly ? readonly_of(e->ctype) : e->ctype;
            return e;
        }
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE || L.sym->type->kind != T_CLASS)
            error_at(e->pos, "class type (or class reference) expected after 'is'/'as'");
        e->ctype = L.sym->type;
        e->type = e->kind == E_IS ? t_bool : e->a->type->readonly ? readonly_of(e->ctype) : e->ctype;
        return e;
    }
    case E_FMT: error_at(e->pos, "format specifiers are only allowed in Write/WriteLn/Str");
    case E_SET: /* [Red, Blue], ['a'..'z'], []: a set, typed by where it is used */
        for (int i = 0; i < e->args.len; i++) {
            e->args.items[i] = value(e->args.items[i]);
            if (!is_ordinal(e->args.items[i]->type)) error_at(e->args.items[i]->pos, "set elements must be ordinal values");
            if (e->args2.items[i]) e->args2.items[i] = value(e->args2.items[i]);
        }
        e->type = t_lit;
        return e;
    case E_LIST: case E_RECLIT: case E_DEFAULT:
        e->type = t_lit; /* typed later from the target (coerce) */
        return e;
    case E_OK: case E_ERR:
        if (e->type && e->type->kind == T_RES) return e;
        e->type = t_lit;
        return e;
    case E_TRY: return try_expr(e);
    case E_ROUTREF: case E_METHREF: return e;
    case E_LAMBDA: return lambda_expr(e);
    default: return e; /* already resolved */
    }
}

/* '=' for records, arrays and optional values: compared element by element */
static int is_aggregate(Type *t) { return t->kind == T_RECORD || t->kind == T_ARRAY || t->kind == T_DYN; }
static int same_agg(Type *a, Type *b) {
    if (a->kind == T_RECORD && b->kind == T_RECORD) return a->st == b->st; /* also 'T const' */
    return same_type(a, b) || (a->kind == b->kind && same_type(a->base, b->base) && a->lo == b->lo && a->hi == b->hi);
}
static int equatable(Type *t) {
    switch (t->kind) {
    case T_INT: case T_BOOL: case T_CHAR: case T_REAL: case T_STR: case T_ENUM: case T_CLASS: case T_PTR: case T_SET: return 1;
    case T_OPT: case T_ARRAY: case T_DYN: return equatable(t->base);
    case T_RECORD:
        for (int i = 0; i < t->st->fields.len; i++)
            if (!equatable(t->st->fields.items[i]->type)) return 0;
        return 1;
    default: return 0;
    }
}

/* ---- operators: class operator Add(a, b: TVec): TVec in records ---- */
static const struct { const char *name; int op; int arity; int cmp; } OPERATORS[] = {
    {"Add", OP_ADD, 2, 0}, {"Subtract", OP_SUB, 2, 0}, {"Multiply", OP_MUL, 2, 0}, {"Divide", OP_DIV, 2, 0},
    {"IntDivide", OP_IDIV, 2, 0}, {"Modulus", OP_MOD, 2, 0}, {"Negative", OP_NEG, 1, 0}, {"Positive", OP_PLUS, 1, 0},
    {"LogicalNot", OP_NOT, 1, 0}, {"Equal", OP_EQ, 2, 1}, {"NotEqual", OP_NE, 2, 1}, {"LessThan", OP_LT, 2, 1},
    {"LessThanOrEqual", OP_LE, 2, 1}, {"GreaterThan", OP_GT, 2, 1}, {"GreaterThanOrEqual", OP_GE, 2, 1}, {NULL, 0, 0, 0}};
static int operator_index(const char *name) {
    for (int i = 0; OPERATORS[i].name; i++)
        if (STREQ(OPERATORS[i].name, name)) return i;
    return -1;
}
static void operator_rules(Struct *s, MethodDecl *md) {
    if (s->kind != SK_RECORD) error_at(md->name.pos, "operators can only be declared in records (values), not in classes");
    if (operator_index(md->name.name) < 0) {
        Buf b = {0};
        for (int i = 0; OPERATORS[i].name; i++) buf_printf(&b, "%s%s", i ? ", " : "", OPERATORS[i].name);
        error_at(md->name.pos, "unknown operator \"%s\" (operators: %s)", md->name.name, buf_str(&b));
    }
    md->is_overload = 1; /* several variants of an operator are chosen by the operand types */
}
static void operator_check(Struct *s, Routine *r) {
    int k = operator_index(r->name);
    r->is_operator = 1;
    if (r->params.len != OPERATORS[k].arity)
        error_at(r->pos, "the operator %s needs %d operand%s", r->name, OPERATORS[k].arity, OPERATORS[k].arity == 1 ? "" : "s");
    int own = 0;
    for (int i = 0; i < r->params.len; i++) {
        Param *pm = &r->params.items[i];
        if (pm->mode != PM_VALUE && pm->mode != PM_CONSTREF)
            error_at(r->pos, "the operands of an operator are value parameters");
        if (pm->type->kind == T_RECORD && pm->type->st == s) own = 1;
    }
    if (!own) error_at(r->pos, "an operator of \"%s\" needs an operand of type \"%s\"", s->name, s->name);
    if (OPERATORS[k].cmp && (!r->ret || r->ret->kind != T_BOOL)) error_at(r->pos, "the comparison operator %s must return Boolean", r->name);
}
/* a + b with a record operand: its operator method (the variant that fits the operand types best) */
static Expr *user_operator(Expr *e, int op, Expr *a, Expr *b) {
    const char *nm = NULL;
    for (int i = 0; OPERATORS[i].name; i++)
        if (OPERATORS[i].op == op && OPERATORS[i].arity == (b ? 2 : 1)) nm = OPERATORS[i].name;
    if (!nm) return NULL;
    VEC(Routine *) cands = {0};
    Type *ts[2] = {a->type, b ? b->type : NULL};
    for (int k = 0; k < 2; k++) {
        Type *t = ts[k];
        if (!t || t->kind != T_RECORD || (k == 1 && ts[0]->kind == T_RECORD && ts[0]->st == t->st)) continue;
        for (int i = 0; i < t->st->methods.len; i++)
            if (t->st->methods.items[i]->is_operator && STREQ(t->st->methods.items[i]->name, nm)) vpush(cands, t->st->methods.items[i]);
    }
    if (!cands.len) return NULL;
    ExprVec args = {0};
    vpush(args, a);
    if (b) vpush(args, b);
    Routine *best = NULL;
    int bestc = 0, tie = 0;
    for (int k = 0; k < cands.len; k++) {
        Routine *c = cands.items[k];
        if (c->params.len != args.len) continue;
        int cost = 0;
        for (int i = 0; i < args.len && cost >= 0; i++) {
            int x = arg_cost(&c->params.items[i], args.items[i]);
            cost = x < 0 ? -1 : cost + x;
        }
        if (cost < 0) continue;
        if (!best || cost < bestc) best = c, bestc = cost, tie = 0;
        else if (cost == bestc) tie = 1;
    }
    if (!best) {
        Buf vb = {0};
        for (int k = 0; k < cands.len; k++) buf_printf(&vb, "%s%s", k ? ", " : "", sig_text(cands.items[k]));
        error_at(e->pos, "no operator %s fits the operands \"%s\"%s%s; there are: %s", nm, type_name(a->type), b ? " and " : "",
                 b ? sfmt("\"%s\"", type_name(b->type)) : "", buf_str(&vb));
    }
    if (tie) error_at(e->pos, "the operator %s is ambiguous for these operands", nm);
    return make_call(best, NULL, &args, CM_NORMAL, NULL, e->pos);
}
static int has_operator(Type *t, const char *nm) {
    if (!t || t->kind != T_RECORD) return 0;
    for (int i = 0; i < t->st->methods.len; i++)
        if (t->st->methods.items[i]->is_operator && STREQ(t->st->methods.items[i]->name, nm)) return 1;
    return 0;
}

/* ---- sets: set of T ---- */
static int set_elem_ok(Type *st, Type *t) {
    return (is_int(t) && is_int(st->base)) || same_type(t, st->base) || (t->kind == T_ENUM && st->base->kind == T_ENUM && t->en == st->base->en);
}
/* the set type of a literal without context: from its first element */
static Type *set_of_elems(Expr *e) {
    if (!e->args.len) error_at(e->pos, "the type of [] is not known here: write the set type ('val s: TColors := []')");
    Type *b = e->args.items[0]->type;
    Type *t = new_type(T_SET, NULL);
    t->base = t->index = b;
    if (b->kind == T_ENUM) t->lo = 0, t->hi = b->en->names.len - 1;
    else if (b->kind == T_BOOL) t->lo = 0, t->hi = 1;
    else t->lo = 0, t->hi = 255; /* Char, numbers: 0..255 */
    if (is_int(b)) t->base = t->index = t_int;
    t->name = b->kind == T_ENUM || b->kind == T_CHAR || b->kind == T_BOOL ? sfmt("set of %s", type_name(b)) : xstrdup("set of 0..255");
    return t;
}
static Expr *set_literal(Expr *e, Type *to) {
    for (int i = 0; i < e->args.len; i++)
        for (int k = 0; k < 2; k++) {
            Expr *x = k ? e->args2.items[i] : e->args.items[i];
            if (!x) continue;
            if (!set_elem_ok(to, x->type))
                error_at(x->pos, "\"%s\" cannot be an element of \"%s\"", type_name(x->type), type_name(to));
            long long v;
            if (lit_ord(x, &v) && (v < to->lo || v > to->hi))
                error_at(x->pos, "%lld is not in the range %lld..%lld of \"%s\"", v, to->lo, to->hi, type_name(to));
        }
    e->type = to;
    return e;
}
static Expr *set_binary(Expr *e) {
    int op = e->op;
    if (e->a->type->kind == T_LIT && e->b->type->kind == T_LIT) e->a = set_literal(e->a, set_of_elems(e->a->args.len ? e->a : e->b));
    if (e->a->type->kind == T_LIT) e->a = coerce(e->a, e->b->type, "set operation");
    if (e->b->type->kind == T_LIT) e->b = coerce(e->b, e->a->type, "set operation");
    Type *ta = e->a->type, *tb = e->b->type;
    if (ta->kind != T_SET || tb->kind != T_SET || !same_type(ta, tb))
        error_at(e->pos, "set operations need two sets of the same type, not \"%s\" and \"%s\"", type_name(ta), type_name(tb));
    switch (op) {
    case OP_ADD: case OP_SUB: case OP_MUL: e->type = ta; return e;
    case OP_EQ: case OP_NE: case OP_LE: case OP_GE: e->type = t_bool; return e;
    default: error_at(e->pos, "this operator is not defined for sets (+ - * = <> <= >=, and 'in')");
    }
}
/* keys of hash maps: values (no objects) whose parts can be hashed */
static int hashable(Type *t) {
    switch (t->kind) {
    case T_INT: case T_BOOL: case T_CHAR: case T_REAL: case T_STR: case T_ENUM: case T_SET: return 1;
    case T_ARRAY: return hashable(t->base);
    case T_RECORD:
        for (int i = 0; i < t->st->fields.len; i++)
            if (!hashable(t->st->fields.items[i]->type)) return 0;
        return 1;
    default: return 0;
    }
}
/* operators on a placeholder type parameter: only what its constraints allow */
static Expr *param_binary(Expr *e) {
    Type *ta = e->a->type, *tb = e->b->type;
    Type *pt = ta->kind == T_PARAM ? ta : tb;
    if (pt->ik & PC_NUM) { /* 'n <= 1', 'n * 2': number literals take the type of the parameter */
        if (ta != pt && (e->a->kind == E_INT || e->a->kind == E_REAL)) e->a->type = ta = pt;
        if (tb != pt && (e->b->kind == E_INT || e->b->kind == E_REAL)) e->b->type = tb = pt;
    }
    int op = e->op;
    const char *need = NULL;
    switch (op) {
    case OP_ADD: case OP_SUB: case OP_MUL: need = (pt->ik & PC_NUM) ? NULL : "Numeric"; e->type = pt; break;
    case OP_EQ: case OP_NE: need = (pt->ik & (PC_EQ | PC_CMP | PC_NUM)) ? NULL : "Equatable"; e->type = t_bool; break;
    case OP_LT: case OP_LE: case OP_GT: case OP_GE:
        need = (pt->ik & (PC_CMP | PC_NUM)) ? NULL : "Comparable";
        e->type = t_bool;
        break;
    default: error_at(e->pos, "operator not applicable to type parameter \"%s\"", type_name(pt));
    }
    if (need) error_at(e->pos, "operator needs the constraint '%s: %s'", type_name(pt), need);
    if (ta != tb)
        error_at(e->pos, "operator not applicable to \"%s\" and \"%s\" (a type parameter only combines with itself)",
                 type_name(ta), type_name(tb));
    return e;
}

/* ---- enumerations with data ---- */
static char *variant_list(Struct *st) {
    Buf b = {0};
    buf_put(&b, "variants: ");
    for (int i = 0; i < st->variants.len; i++) {
        VariantInfo *v = &st->variants.items[i];
        if (i) buf_put(&b, ", ");
        buf_put(&b, v->name);
        if (v->n) buf_put(&b, "(...)");
    }
    return xstrdup(buf_str(&b));
}
static char *variant_sig(Struct *st, int vi) {
    VariantInfo *v = &st->variants.items[vi];
    Buf b = {0};
    buf_printf(&b, "%s(", v->name);
    for (int i = 0; i < v->n; i++) {
        Field *f = st->fields.items[v->first + i];
        buf_printf(&b, "%s%s: %s", i ? "; " : "", strchr(f->name, '.') + 1, type_name(f->type));
    }
    buf_put(&b, ")");
    return xstrdup(buf_str(&b));
}
/* Circle(2.0): a record literal with the tag and the fields of the variant */
static Expr *variant_value(Type *t, int vi, ExprVec *args, Pos p) {
    Struct *st = t->st;
    VariantInfo *v = &st->variants.items[vi];
    int n = args ? args->len : 0;
    if (n != v->n) {
        if (!v->n) error_at(p, "\"%s\" has no data: write '%s' without parentheses", v->name, v->name);
        error_at(p, "\"%s\" needs %d value%s: %s", v->name, v->n, v->n == 1 ? "" : "s", variant_sig(st, vi));
    }
    Expr *r = new_expr(E_RECLIT, p);
    vpush(r->fnames, ((Ident){"#tag", p}));
    vpush(r->args, lit_int(vi, t_int, p));
    for (int i = 0; i < n; i++) {
        vpush(r->fnames, ((Ident){st->fields.items[v->first + i]->name, args->items[i]->pos}));
        vpush(r->args, args->items[i]);
    }
    return literal_to(r, t, "variant");
}
static int find_variant(Struct *st, Ident id) {
    for (int i = 0; i < st->variants.len; i++)
        if (STREQ(st->variants.items[i].name, id.name)) return i;
    error_at(id.pos, "\"%s\" is not a variant of \"%s\" (%s)", id.name, st->name, variant_list(st));
}
static Expr *tag_is(Expr *x, int vi, Pos p) {
    Expr *m = new_expr(E_MEMBER, p);
    m->a = x;
    m->id = (Ident){"#tag", p};
    Expr *c = new_expr(E_BINARY, p);
    c->op = OP_EQ;
    c->a = m;
    c->b = lit_int(vi, t_int, p);
    return c;
}
/* 's is Empty' (a test without bindings); 's is Circle(r)' with bindings only as the condition of 'if' */
static Expr *variant_test(Expr *e) {
    Struct *st = e->a->type->st;
    int vi = find_variant(st, e->id);
    if (e->has_args && e->args.len)
        error_at(e->pos, "a pattern with names ('is %s(...)') can only be the whole condition of an 'if' (or use 'case')", e->id.name);
    if (!is_lvalue(e->a) && (has_owned(e->a->type) || has_str(e->a->type)))
        error_at(e->pos, "store the value in a variable before testing its variant");
    return value(tag_is(e->a, vi, e->pos));
}
/* case s of Circle(r): A; Rect(w, h): B; Empty: C end
   ==>  begin var __s := s; if __s.#tag = 0 then var r := __s.Circle.Radius; A else if ... end */
static void sum_case(Stmt *s, int from_if) {
    Pos p = s->pos;
    Type *t = s->a->type;
    Struct *st = t->st;
    int nv = st->variants.len;
    int *seen = (int *)xcalloc(nv ? nv : 1, sizeof(int));
    int any_var = 0;
    for (int i = 0; i < s->arms.len; i++) {
        CaseArm *arm = &s->arms.items[i];
        for (int k = 0; k < arm->lo.len; k++) {
            Expr *l = arm->lo.items[k];
            if (arm->hi.items[k]) error_at(l->pos, "variants cannot be ranges");
            Ident id;
            ExprVec *binds = NULL;
            if (l->kind == E_CALL && l->a->kind == E_IDENT) id = l->a->id, binds = &l->args;
            else if (l->kind == E_IDENT) id = l->id;
            else error_at(l->pos, "a variant of \"%s\" expected (%s)", st->name, variant_list(st));
            int vi = find_variant(st, id);
            if (seen[vi]) error_at(l->pos, "variant \"%s\" is handled twice", id.name);
            seen[vi] = 1;
            VariantInfo *v = &st->variants.items[vi];
            if (binds) {
                if (arm->lo.len > 1) error_at(l->pos, "an arm with names can only have one variant");
                if (binds->len != v->n)
                    error_at(l->pos, "\"%s\" has %d field%s: %s", v->name, v->n, v->n == 1 ? "" : "s", variant_sig(st, vi));
                for (int b = 0; b < binds->len; b++) {
                    Expr *x = binds->items[b];
                    if (x->kind != E_IDENT || x->targs.len) error_at(x->pos, "a name expected in the pattern (or '_')");
                    if (x->pat_var) any_var = 1;
                }
            }
        }
    }
    if (!s->has_else && !from_if) {
        Buf b = {0};
        int miss = 0;
        for (int i = 0; i < nv; i++)
            if (!seen[i]) buf_printf(&b, "%s%s", miss++ ? ", " : "", st->variants.items[i].name);
        if (miss)
            error_at(p, "case does not handle the variant%s %s of \"%s\" (add %s or an 'else')", miss > 1 ? "s" : "", buf_str(&b),
                     st->name, miss > 1 ? "them" : "it");
    }
    if (s->has_else && !from_if) {
        int all = 1;
        for (int i = 0; i < nv; i++) all &= seen[i];
        if (all) error_at(p, "'else' is unreachable: all variants of \"%s\" are handled", st->name);
    }
    /* the selector: a reference to the variable, or a hidden owner of a computed value */
    char *hn = sfmt("__sum%d_%d", p.line, p.col);
    Block *ob = NEW(Block);
    Decl *d = NEW(Decl);
    d->kind = D_VAR;
    VarDecl hv = {0};
    vpush(hv.names, ((Ident){hn, p}));
    hv.init = s->a;
    if (is_lvalue(s->a)) {
        hv.borrowed = 1;
        hv.ro = !any_var;
    }
    vpush(d->vars, hv);
    vpush(ob->decls, d);
    Stmt *chain = NULL, **tail = &chain;
    for (int i = 0; i < s->arms.len; i++) {
        CaseArm *arm = &s->arms.items[i];
        Expr *c = NULL;
        Block *ab = NEW(Block);
        Decl *bd = NEW(Decl);
        bd->kind = D_VAR;
        for (int k = 0; k < arm->lo.len; k++) {
            Expr *l = arm->lo.items[k];
            Ident id = l->kind == E_CALL ? l->a->id : l->id;
            int vi = find_variant(st, id);
            Expr *ti = tag_is(mk_ident(hn, l->pos), vi, l->pos);
            c = c ? mk_bin(OP_OR, c, ti, l->pos) : ti;
            if (l->kind != E_CALL) continue;
            VariantInfo *v = &st->variants.items[vi];
            for (int b = 0; b < l->args.len; b++) {
                Expr *x = l->args.items[b];
                if (STREQ(x->id.name, "_")) continue;
                VarDecl bv = {0};
                vpush(bv.names, x->id);
                Expr *m = new_expr(E_MEMBER, x->pos);
                m->a = mk_ident(hn, x->pos);
                m->id = (Ident){st->fields.items[v->first + b]->name, x->pos};
                bv.init = m;
                bv.borrowed = 1;
                bv.ro = !x->pat_var;
                vpush(bd->vars, bv);
            }
        }
        Stmt *body = arm->body;
        if (bd->vars.len) {
            vpush(ab->decls, bd);
            vpush(ab->body, arm->body);
            body = new_stmt(S_BLOCK, arm->body->pos);
            body->blk = ab;
        }
        Stmt *n = new_stmt(S_IF, l_pos(arm));
        n->a = c;
        n->s1 = body;
        *tail = n;
        tail = &n->s2;
    }
    if (s->has_else) {
        Stmt *c = new_stmt(S_COMPOUND, p);
        c->body = s->else_;
        *tail = c;
    }
    if (!chain) chain = new_stmt(S_EMPTY, p);
    vpush(ob->body, chain);
    Stmt *blk = new_stmt(S_BLOCK, p);
    blk->blk = ob;
    *s = *blk;
    stmt(s);
}

/* ---- for val x in collection: rewritten into an index loop ---- */
static Expr *mk_ident(const char *name, Pos p) {
    Expr *e = new_expr(E_IDENT, p);
    e->id = (Ident){(char *)name, p};
    return e;
}
static Expr *mk_int(long long v, Pos p) {
    Expr *e = new_expr(E_INT, p);
    e->ival = v;
    return e;
}
static Expr *mk_bin(int op, Expr *a, Expr *b, Pos p) {
    Expr *e = new_expr(E_BINARY, p);
    e->op = op;
    e->a = a;
    e->b = b;
    return e;
}
static Expr *mk_member(Expr *base, const char *name, ExprVec *args, Pos p) {
    Expr *m = new_expr(E_MEMBER, p);
    m->a = base;
    m->id = (Ident){(char *)name, p};
    if (!args) return m;
    Expr *c = new_expr(E_CALL, p);
    c->a = m;
    c->args = *args;
    return c;
}

/* ---- for val x in c with c.GetEnumerator (MoveNext, Current) ---- */
static Routine *enumerator_of(Struct *st) {
    Field *f;
    Routine *m;
    Prop *pr;
    if (find_member(st, "GetEnumerator", &f, &m, &pr) && m && m->kind == RK_FUNC && !m->params.len && !m->is_classm) return m;
    return NULL;
}
/* begin  var __src := c;  val __e := c.GetEnumerator;
          while __e.MoveNext and __Touch(__src) do  <x := __e.Current>  body  end */
static void for_enumerator(Stmt *s, Expr *coll, Routine *ge) {
    Pos p = s->pos;
    Type *et = ge->ret;
    if (!et || (et->kind != T_CLASS && et->kind != T_RECORD))
        error_at(s->a->pos, "GetEnumerator of \"%s\" must return a record or an object", type_name(coll->type));
    Field *f;
    Routine *mn, *cm;
    Prop *cp;
    if (!find_member(et->st, "MoveNext", &f, &mn, &cp) || !mn || mn->kind != RK_FUNC || mn->params.len || !mn->ret || mn->ret->kind != T_BOOL)
        error_at(s->a->pos, "the enumerator \"%s\" needs 'function MoveNext: Boolean'", type_name(et));
    Type *ct = NULL;
    int borrowed = 0;
    if (find_member(et->st, "Current", &f, &cm, &cp) && cm && cm->kind == RK_FUNC && !cm->params.len) ct = cm->ret, borrowed = cm->ret_borrowed;
    else if (cp && !cp->idx.len) {
        ct = cp->type;
        Routine *rd;
        if (cp->read && find_member(et->st, cp->read, &f, &rd, &cp) && rd) borrowed = rd->ret_borrowed;
    }
    if (!ct) error_at(s->a->pos, "the enumerator \"%s\" needs 'function Current: T' or 'property Current: T'", type_name(et));
    char *src = sfmt("__src%d_%d", p.line, p.col), *en = sfmt("__en%d_%d", p.line, p.col);
    Block *ob = NEW(Block);
    Decl *d = NEW(Decl);
    d->kind = D_VAR;
    VarDecl sv = {0}; /* the collection, borrowed read-only for the whole loop */
    vpush(sv.names, ((Ident){src, p}));
    sv.init = coll;
    if (is_lvalue(coll)) {
        sv.borrowed = 1;
        sv.ro = 1;
    }
    if (!is_lvalue(coll)) vpush(d->vars, sv); /* a computed collection: a hidden copy first, the enumerator from it */
    VarDecl ev = {0};
    vpush(ev.names, ((Ident){en, p}));
    Expr *call = new_expr(E_CALL, p);
    call->a = new_expr(E_MEMBER, p);
    call->a->a = is_lvalue(coll) ? coll : mk_ident(src, p);
    call->a->id = (Ident){"GetEnumerator", p};
    ev.init = call;
    ev.is_heap = et->kind == T_CLASS; /* an enumerator object: owned by the loop */
    ev.ctor = NULL;
    vpush(d->vars, ev);
    if (is_lvalue(coll)) vpush(d->vars, sv); /* a variable: borrowed after the enumerator is made, for the whole loop */
    vpush(ob->decls, d);
    /* the element */
    Expr *cur = new_expr(E_MEMBER, p);
    cur->a = mk_ident(en, p);
    cur->id = (Ident){"Current", p};
    Stmt *body;
    if (is_optional(ct)) {
        Expr *is = new_expr(E_ISSOME, p);
        is->a = cur;
        is->id = s->var;
        is->has_id = 1;
        is->ro_bind = s->decl_var == 1;
        body = new_stmt(S_IF, p);
        body->a = is;
        body->s1 = s->s1;
    } else {
        if (s->decl_var == 2 && !borrowed && ct->kind != T_CLASS)
            error_at(s->pos, "Current of \"%s\" gives a copy: write 'for val %s in ...' (or let Current return 'var T')", type_name(et), s->var.name);
        Block *b = NEW(Block);
        Decl *bd = NEW(Decl);
        bd->kind = D_VAR;
        VarDecl xv = {0};
        vpush(xv.names, s->var);
        xv.init = cur;
        xv.borrowed = borrowed || ct->kind == T_CLASS;
        xv.ro = s->decl_var == 1;
        vpush(bd->vars, xv);
        vpush(b->decls, bd);
        vpush(b->body, s->s1);
        body = new_stmt(S_BLOCK, p);
        body->blk = b;
    }
    Expr *mv = new_expr(E_CALL, p);
    mv->a = new_expr(E_MEMBER, p);
    mv->a->a = mk_ident(en, p);
    mv->a->id = (Ident){"MoveNext", p};
    Expr *touch = new_expr(E_CALL, p);
    touch->a = mk_ident("__Touch", p);
    vpush(touch->args, mk_ident(src, p));
    Stmt *w = new_stmt(S_WHILE, p);
    w->a = mk_bin(OP_AND, mv, touch, p);
    w->s1 = body;
    vpush(ob->body, w);
    Stmt *blk = new_stmt(S_BLOCK, p);
    blk->blk = ob;
    *s = *blk;
    forin_call = call;
    stmt(s);
    forin_call = NULL;
}
static int find_member(Struct *st, const char *name, Field **f, Routine **m, Prop **p);
static void for_in(Stmt *s) {
    Pos p = s->pos;
    Expr *coll = value(s->a);
    Type *t = coll->type;
    char *iname = sfmt("__in%d_%d", p.line, p.col);
    Expr *lo, *hi, *elem;
    Type *et = NULL;
    if (t->kind == T_ARRAY || t->kind == T_DYN || t->kind == T_STR) {
        if (!is_lvalue(coll))
            error_at(s->a->pos, "'for ... in' needs a variable or field here; store the value in a variable first");
        if (t->kind == T_ARRAY) {
            lo = lit_int(t->lo, t->index, p);
            hi = lit_int(t->hi, t->index, p);
        } else {
            ExprVec a1 = {0};
            vpush(a1, coll);
            Expr *len = new_expr(E_CALL, p);
            len->a = mk_ident("Length", p);
            len->args = a1;
            lo = mk_int(t->kind == T_STR ? 1 : 0, p);
            hi = t->kind == T_STR ? len : mk_bin(OP_SUB, len, mk_int(1, p), p);
        }
        et = t->kind == T_STR ? NULL : t->base;
        elem = new_expr(E_INDEX, p);
        elem->a = coll;
        vpush(elem->args, mk_ident(iname, p));
    } else if (t->kind == T_SET) { /* for val c in colors: the members in order */
        if (s->decl_var != 1) error_at(s->pos, "the elements of a set are values: write 'for val %s in ...'", s->var.name);
        Stmt *loop = new_stmt(S_FOR, p);
        loop->decl_var = 1;
        loop->var = (Ident){iname, p};
        loop->a = mk_int(t->lo, p);
        loop->b = mk_int(t->hi, p);
        Expr *cast = new_expr(E_CALL, p); /* TColor(__i) */
        Expr *tn = new_expr(E_TYPEREF, p);
        tn->ctype = t->base;
        tn->type = t->base;
        cast->a = tn;
        vpush(cast->args, mk_ident(iname, p));
        Block *b = NEW(Block);
        Decl *d = NEW(Decl);
        d->kind = D_VAR;
        VarDecl vd = {0};
        vpush(vd.names, s->var);
        vd.init = cast;
        vpush(d->vars, vd);
        vpush(b->decls, d);
        vpush(b->body, s->s1);
        Stmt *blk = new_stmt(S_BLOCK, p);
        blk->blk = b;
        Expr *in = new_expr(E_IN, p);
        Expr *cast2 = new_expr(E_CALL, p);
        cast2->a = tn;
        vpush(cast2->args, mk_ident(iname, p));
        in->a = cast2;
        in->b = coll;
        Stmt *iff = new_stmt(S_IF, p);
        iff->a = in;
        iff->s1 = blk;
        loop->s1 = iff;
        *s = *loop;
        stmt(s);
        return;
    } else if ((t->kind == T_CLASS || t->kind == T_RECORD) && enumerator_of(t->st)) {
        for_enumerator(s, coll, enumerator_of(t->st));
        return;
    } else if (t->kind == T_CLASS || t->kind == T_RECORD) { /* a container with Count and Get(i) */
        Field *f;
        Routine *m;
        Prop *pr;
        if (!find_member(t->st, "Count", &f, &m, &pr) || !m || !find_member(t->st, "Get", &f, &m, &pr) || !m)
            error_at(s->a->pos, "\"%s\" cannot be used in 'for ... in': it needs the methods 'Count' and 'Get(i)'", type_name(t));
        if (!is_lvalue(coll))
            error_at(s->a->pos, "'for ... in' needs a variable or field here; store the container in a variable first");
        lo = mk_int(0, p);
        hi = mk_bin(OP_SUB, mk_member(coll, "Count", NULL, p), mk_int(1, p), p);
        ExprVec a1 = {0};
        vpush(a1, mk_ident(iname, p));
        et = m->ret;
        elem = mk_member(coll, "Get", &a1, p);
    } else
        error_at(s->a->pos, "\"%s\" cannot be used in 'for ... in' (arrays, strings and containers with Count/Get)", type_name(t));
    /* the element: unpacked if optional, else a reference (read-only for 'for val') */
    Stmt *body;
    if (et && is_optional(et)) {
        Expr *is = new_expr(E_ISSOME, p);
        is->a = elem;
        is->id = s->var;
        is->has_id = 1;
        is->ro_bind = s->decl_var == 1;
        body = new_stmt(S_IF, p);
        body->a = is;
        body->s1 = s->s1;
    } else {
        Block *b = NEW(Block);
        Decl *d = NEW(Decl);
        d->kind = D_VAR;
        VarDecl vd = {0};
        vpush(vd.names, s->var);
        vd.init = elem;
        vd.borrowed = 1;
        vd.ro = s->decl_var == 1;
        vpush(d->vars, vd);
        vpush(b->decls, d);
        vpush(b->body, s->s1);
        body = new_stmt(S_BLOCK, p);
        body->blk = b;
    }
    Stmt *f = new_stmt(S_FOR, p);
    f->decl_var = 1;
    f->var = (Ident){iname, p};
    f->a = lo;
    f->b = hi;
    f->s1 = body;
    *s = *f;
    stmt(s);
}

/* ---- function values ---- */
static Type *proc_type(Routine *sig) {
    Type *t = new_type(T_PROC, NULL);
    t->proc = sig;
    t->name = (char *)proc_name(sig);
    return t;
}
static void closure_of(Type *t) {
    t->closure = 1;
    t->name = sfmt("reference to %s", proc_name(t->proc));
}

/* a method value only lives as long as the call it is passed to: it may not be stored (its object could be freed) */
static void no_method_value(Expr *e) {
    if (e && e->kind == E_METHREF)
        error_at(e->pos, "a method value can only be passed directly as an argument ('%s' could be freed while the value is kept); use a lambda that calls it",
                 e->a->kind == E_VAR ? e->a->var->name : "its object");
}
/* obj.Show as a function value: the method together with its object (a closure whose frame is the object) */
static Expr *method_value(Routine *m, Expr *base, int mode, Pos p) {
    if (m->is_overload) error_at(p, "\"%s\" is overloaded and cannot be used as a value (wrap the call in a lambda)", m->name);
    if (m->is_classm || m->kind == RK_CTOR || m->kind == RK_DTOR)
        error_at(p, "only ordinary methods can be used as values ('%s' is a %s)", m->name,
                 m->is_classm ? "class method" : m->kind == RK_CTOR ? "constructor" : "destructor");
    if (base->type->kind == T_CLASS && is_owning_temp(base))
        error_at(p, "the object of a method value must be a variable (this one would be freed at once): store it first");
    if (base->type->kind == T_RECORD && !is_lvalue(base)) error_at(p, "the record of a method value must be a variable");
    if (!m->is_constm && ((base->type->kind == T_CLASS && base->type->readonly) || rooted_const(base)))
        error_at(p, "'%s' is read-only: only const methods can be used as values of it", const_root_name(base));
    Expr *e = new_expr(E_METHREF, p);
    e->rt = m;
    e->a = base;
    e->cmode = mode;
    e->type = proc_type(m);
    closure_of(e->type);
    m->addr_taken = 1; /* calls through function values may reach it (borrow checker) */
    return e;
}
static Expr *routine_ref(Routine *rt, Pos p) {
    if (rt->owner || rt->is_sig)
        error_at(p, "only routines (not methods) can be used as values ('%s' is a method: wrap the call in a lambda)", rt->name);
    Expr *e = new_expr(E_ROUTREF, p);
    e->rt = rt;
    e->type = proc_type(rt);
    if (rt->level > 1) closure_of(e->type); /* a nested routine refers to the frame of the routine around it */
    rt->addr_taken = 1;
    return e;
}
static Expr *make_call(Routine *rt, Expr *base, ExprVec *args, int mode, Type *newcls, Pos p);
static Expr *indirect_call(Expr *callee, ExprVec *args, Pos p) {
    Expr *c = make_call(callee->type->proc, NULL, args, CM_NORMAL, NULL, p);
    c->cmode = CM_INDIRECT;
    c->b = callee;
    return c;
}
static void routine_body(Routine *rt, RoutineDecl *d);
static void build_params(Routine *r, ParamGroupVec *groups);
static Routine *new_routine(const char *name, int kind, Pos p);
/* function(a, b: Integer): Integer begin ... end */

/* 'reference to function(...)' values may be closures that point into the frame of a routine:
   they may only be value parameters and local variables of routines, never stored anywhere else */
static void no_closure(Type *t, Pos p, const char *where) {
    if (t && t->kind == T_PROC && t->closure)
        error_at(p, "%s cannot be \"%s\": a closure may refer to the variables of a routine, so it can only be a value parameter or a local variable of a routine",
                 where, type_name(t));
}

/* does a lambda use variables (or Self, or nested routines) of the routine around it? */
typedef struct {
    VEC(const char *) decl; /* names declared in the lambda */
    VEC(Expr *) used;       /* identifiers it uses */
    int self;               /* inherited / Self */
} CapScan;
static void cap_expr(Expr *e, CapScan *c);
static void cap_stmt(Stmt *s, CapScan *c);
static void cap_block(Block *b, CapScan *c) {
    if (!b) return;
    for (int i = 0; i < b->decls.len; i++) {
        Decl *d = b->decls.items[i];
        for (int k = 0; k < d->vars.len; k++) {
            VarDecl *v = &d->vars.items[k];
            for (int n = 0; n < v->names.len; n++) vpush(c->decl, v->names.items[n].name);
            cap_expr(v->init, c);
            cap_expr(v->ctor, c);
        }
        for (int k = 0; k < d->consts.len; k++) {
            vpush(c->decl, d->consts.items[k].name.name);
            cap_expr(d->consts.items[k].value, c);
        }
        if (d->rt) vpush(c->decl, d->rt->name.name);
    }
    for (int i = 0; i < b->body.len; i++) cap_stmt(b->body.items[i], c);
}
static void cap_rdecl(RoutineDecl *d, CapScan *c) {
    for (int i = 0; i < d->params.len; i++)
        for (int n = 0; n < d->params.items[i].names.len; n++) vpush(c->decl, d->params.items[i].names.items[n].name);
    vpush(c->decl, "Result");
    cap_block(d->body, c);
}
static void cap_expr(Expr *e, CapScan *c) {
    if (!e) return;
    if (e->kind == E_IDENT) vpush(c->used, e);
    if (e->kind == E_INHERITED) c->self = 1;
    if ((e->kind == E_ISSOME || e->kind == E_SOMEPAT) && e->has_id) vpush(c->decl, e->id.name);
    if (e->kind == E_LAMBDA && e->lam) cap_rdecl((RoutineDecl *)e->lam, c);
    cap_expr(e->a, c);
    cap_expr(e->b, c);
    cap_expr(e->c, c);
    for (int i = 0; i < e->args.len; i++) cap_expr(e->args.items[i], c);
    for (int i = 0; i < e->args2.len; i++) cap_expr(e->args2.items[i], c);
}
static void cap_stmt(Stmt *s, CapScan *c) {
    if (!s) return;
    if ((s->kind == S_FOR && s->decl_var) || s->kind == S_FORIN) vpush(c->decl, s->var.name);
    else if (s->kind == S_FOR) {
        Expr *v = new_expr(E_IDENT, s->pos);
        v->id = s->var;
        vpush(c->used, v);
    }
    cap_expr(s->a, c);
    cap_expr(s->b, c);
    cap_expr(s->c, c);
    cap_stmt(s->s1, c);
    cap_stmt(s->s2, c);
    for (int i = 0; i < s->body.len; i++) cap_stmt(s->body.items[i], c);
    for (int i = 0; i < s->arms.len; i++) {
        for (int k = 0; k < s->arms.items[i].lo.len; k++) cap_expr(s->arms.items[i].lo.items[k], c);
        for (int k = 0; k < s->arms.items[i].hi.len; k++) cap_expr(s->arms.items[i].hi.items[k], c);
        cap_stmt(s->arms.items[i].body, c);
    }
    for (int i = 0; i < s->else_.len; i++) cap_stmt(s->else_.items[i], c);
    for (int i = 0; i < s->exprs.len; i++) cap_expr(s->exprs.items[i], c);
    cap_block(s->blk, c);
}
static int lambda_captures(RoutineDecl *d) {
    CapScan c = {0};
    cap_rdecl(d, &c);
    if (c.self) return 1;
    for (int i = 0; i < c.used.len; i++) {
        const char *n = c.used.items[i]->id.name;
        int own = 0;
        for (int k = 0; k < c.decl.len && !own; k++) own = STREQ(c.decl.items[k], n);
        if (own) continue;
        Look L = lookup(n);
        if (L.kind == LK_MEMBER) return 1; /* a member of Self */
        if (L.kind != LK_SYM) continue;
        if (L.sym->kind == SY_VAR && !L.sym->var->is_global) return 1;
        if (L.sym->kind == SY_ROUTINE && L.sym->rt->level > 1) return 1;
    }
    return 0;
}
static Expr *lambda_expr(Expr *e) {
    if (e->rt) return e;
    RoutineDecl *d = e->lam;
    Routine *rt = new_routine("lambda", d->kind, d->pos);
    rt->level = 1;
    rt->cname = sfmt("LAM%d", rt->id);
    build_params(rt, &d->params);
    if (d->kind == RK_FUNC) rt->ret = resolve_type(d->ret);
    if (rt->ret) no_closure(rt->ret, d->pos, "the result of a lambda");
    rt->addr_taken = 1;
    if (cur && lambda_captures(d)) { /* a closure: a nested routine that uses the frame of the routine around it */
        rt->level = cur->level + 1;
        rt->parent = cur;
        rt->captures = 1;
        Expr *st = try_site;
        int sloop = loop_depth, sb = in_block;
        in_block = 0;
        routine_body(rt, d);
        in_block = sb;
        loop_depth = sloop;
        try_site = st;
        e->rt = rt;
        e->type = proc_type(rt);
        closure_of(e->type);
        return e;
    }
    Scope *ss = scope, *so = lambda_outer;
    Routine *sc = cur;
    int sl = level, sb = in_block, sloop = loop_depth;
    Expr *st = try_site;
    Struct *sic = inst_cls;
    lambda_outer = scope;
    if (cur) scope = top_scope; /* no access to the locals of the routine around it */
    cur = NULL;
    level = 0;
    in_block = 0;
    inst_cls = NULL;
    routine_body(rt, d);
    scope = ss;
    lambda_outer = so;
    cur = sc;
    level = sl;
    in_block = sb;
    loop_depth = sloop;
    try_site = st;
    inst_cls = sic;
    e->rt = rt;
    e->type = proc_type(rt);
    return e;
}

/* 'try r': the value of a successful result; a failed one ends the function with its error */
static Expr *try_site;
static Expr *try_expr(Expr *e) {
    if (e->type && e->type->kind != T_LIT && e->a->type) return e;
    if (e != try_site)
        error_at(e->pos, "'try' is only allowed as a whole statement, as the value of an assignment or as an initializer");
    try_site = NULL;
    e->a = value(e->a);
    Type *t = e->a->type;
    if (t->kind != T_RES) error_at(e->a->pos, "'try' needs a result, not \"%s\"", type_name(t));
    Type *rt = cur && cur->result ? cur->result->type : NULL;
    if (!rt || rt->kind != T_RES)
        error_at(e->pos, "'try' passes the error to the caller, so it needs a function that returns a result ('function F: result T%s')",
                 t->err->kind == T_STR ? "" : sfmt(" of %s", type_name(t->err)));
    if (!same_type(rt->err, t->err))
        error_at(e->pos, "'try' cannot pass an error of type \"%s\" on from a function whose errors are \"%s\"",
                 type_name(t->err), type_name(rt->err));
    if (t->base && is_ref_type(t->base) && !res_temp(e->a))
        error_at(e->pos, "'try' on a stored result would take its object: use 'if r is ok x' or call the function directly ('try F(...)')");
    e->type = t->base ? t->base : t_void;
    return e;
}

static Expr *value(Expr *e) {
    Expr *r = ex(e);
    if (r->kind == E_TYPEREF && r->ctype->kind == T_CLASS && !r->ctype->st->is_iface && !r->ctype->nilable && !r->ctype->readonly) {
        Expr *c = new_expr(E_TYPEREF, r->pos); /* TCircle as a value: its class reference */
        c->ctype = r->ctype;
        c->type = classref_of(r->ctype->st);
        return c;
    }
    if (r->kind == E_TYPEREF) error_at(e->pos, "type \"%s\" used as a value", type_name(r->ctype));
    if (r->type == t_void) error_at(e->pos, "procedure call used as a value");
    return r;
}

/* ============================================================== builtins */

/* a literal format of Format: the specifiers must match the arguments */
static void format_check(Expr *f, Expr *e, Pos p) {
    int n = e->args.len - 1, next = 0, used = 0;
    const char *s = f->sval;
    int len = f->slen;
    for (int i = 0; i < len; i++) {
        if (s[i] != '%') continue;
        if (++i >= len) error_at(f->pos, "Format: '%%' at the end of the format");
        if (s[i] == '%') continue;
        int j = i, num = 0, has = 0;
        while (j < len && isdigit((unsigned char)s[j])) num = num * 10 + (s[j++] - '0'), has = 1;
        if (has && j < len && s[j] == ':') { next = num; i = j + 1; }
        if (i < len && s[i] == '-') i++;
        while (i < len && isdigit((unsigned char)s[i])) i++;
        if (i < len && s[i] == '.') { i++; while (i < len && isdigit((unsigned char)s[i])) i++; }
        if (i >= len) error_at(f->pos, "Format: incomplete format specifier at the end");
        char t = (char)tolower((unsigned char)s[i]);
        if (!strchr("duxfegns", t)) error_at(f->pos, "Format: unknown specifier '%%%c' (known: %%d %%u %%x %%f %%e %%g %%n %%s %%%%)", s[i]);
        if (next >= n) error_at(p, "Format: the format needs more arguments than the %d given", n);
        Expr *x = e->args.items[1 + next];
        Type *at = x->type;
        if (strchr("dux", t) && !is_int(at))
            error_at(x->pos, "Format: '%%%c' needs a whole number, but argument %d is \"%s\"", s[i], next + 1, type_name(at));
        if (strchr("fegn", t) && !is_int(at) && at->kind != T_REAL)
            error_at(x->pos, "Format: '%%%c' needs a number, but argument %d is \"%s\"", s[i], next + 1, type_name(at));
        if (t == 's' && (is_int(at) || at->kind == T_REAL))
            error_at(x->pos, "Format: '%%s' needs a string, but argument %d is \"%s\" (use %%d or %%f)", next + 1, type_name(at));
        next++;
        if (next > used) used = next;
    }
    if (used < n) error_at(p, "Format: %d argument%s given, but the format uses only %d", n, n == 1 ? "" : "s", used);
}

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
    {"FloatToStr", BI_FLOATTOSTR}, {"Format", BI_FORMAT}, {"__Touch", BI_TOUCH}, {"Hash", BI_HASH}, {"Include", BI_INCLUDE}, {"Exclude", BI_EXCLUDE}, {"Str", BI_STR}, {"UpCase", BI_UPCASE}, {"UpperCase", BI_UPPERCASE},
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
        if (!is_strlike(a->type) && a->type->kind != T_DYN) error_at(p, "Length needs a string or array");
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
        case T_DYN:
            if (!hi) return lit_int(0, t_int, p);
            vpush(e->args, value(a)); /* High(a) = Length(a) - 1 */
            e->type = t_int;
            return e;
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
    case BI_TOUCH: /* made by 'for ... in': a use of the collection in every step (borrow checker) */
        NARGS(1, 1);
        vpush(e->args, value(A(0)));
        e->type = t_bool;
        return e;
    case BI_HASH: { /* Hash(x): Int64 for keys of hash maps */
        NARGS(1, 1);
        Expr *x = value(A(0));
        Type *t = x->type;
        if (t->kind == T_PARAM) {
            if (!(t->ik & (PC_HASH | PC_CMP | PC_NUM))) error_at(x->pos, "Hash needs the constraint '%s: Hashable'", type_name(t));
        } else if (!hashable(t))
            error_at(x->pos, "\"%s\" cannot be hashed (numbers, Char, Boolean, enumerations, strings, sets and records or arrays of them can)",
                     type_name(t));
        vpush(e->args, x);
        e->type = t_i64;
        return e;
    }
    case BI_INCLUDE: case BI_EXCLUDE: { /* Include(s, Red) / Exclude(s, Red) */
        NARGS(2, 2);
        Expr *st = value(A(0));
        if (st->type->kind != T_SET) error_at(st->pos, "%s needs a set variable", nm);
        check_writable(st, sfmt("the set of %s", nm));
        Expr *x = value(A(1));
        if (!set_elem_ok(st->type, x->type)) error_at(x->pos, "\"%s\" is not an element type of \"%s\"", type_name(x->type), type_name(st->type));
        vpush(e->args, st);
        vpush(e->args, x);
        e->type = t_void;
        return e;
    }
    case BI_FORMAT: { /* Format('%d of %s', [n, s]) */
        NARGS(2, 2);
        Expr *f = coerce(value(A(0)), t_str, nm);
        vpush(e->args, f);
        Expr *l = A(1);
        if (l->kind != E_SET) error_at(l->pos, "Format needs its arguments in brackets: Format('%%d %%s', [n, s])");
        for (int i = 0; i < l->args.len; i++) {
            if (l->args2.items[i]) error_at(l->args.items[i]->pos, "ranges are not allowed in the arguments of Format");
            Expr *x = value(l->args.items[i]);
            Type *t = x->type;
            if (!(is_int(t) || t->kind == T_REAL || t->kind == T_STR || t->kind == T_CHAR || t->kind == T_BOOL || t->kind == T_ENUM))
                error_at(x->pos, "Format cannot show \"%s\" (numbers, strings, characters, Boolean and enumerations can)", type_name(t));
            vpush(e->args, x);
        }
        if (f->kind == E_STR) format_check(f, e, l->pos);
        e->type = t_str;
        return e;
    }
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
    case BI_BOXNEW:
        NARGS(0, 1);
        if (n) vpush(e->args, value(A(0)));
        return e;
    case BI_ISOK: case BI_ISERROR: {
        NARGS(1, 1);
        vpush(e->args, A(0));
        e->type = t_bool;
        return e;
    }
    case BI_HASVALUE: {
        NARGS(1, 1);
        vpush(e->args, A(0));
        e->type = t_bool;
        return e;
    }
    case BI_VALUEOR: {
        NARGS(2, 2);
        if (A(0)->type->kind == T_RES && !A(0)->type->base)
            error_at(p, "\"%s\" has no value, ValueOr is not available", type_name(A(0)->type));
        if (A(0)->type->kind == T_RES && is_ref_type(A(0)->type->base))
            error_at(p, "ValueOr is not available for a result with an object (use 'if r is ok x' or 'case')");
        if (A(0)->type->kind != T_OPT && A(0)->type->kind != T_RES)
            error_at(p, "ValueOr is only available for optional values, not for optional objects (use 'is some')");
        vpush(e->args, A(0));
        vpush(e->args, coerce(value(A(1)), A(0)->type->base, "ValueOr"));
        e->type = A(0)->type->base;
        return e;
    }
    case BI_ASSIGNED: {
        NARGS(1, 1);
        Expr *a = value(A(0));
        if (!is_ptrlike(a->type) && a->type->kind != T_OPT) error_at(p, "class instance or optional value expected");
        vpush(e->args, a);
        e->type = t_bool;
        return e;
    }
    case BI_NEW: case BI_DISPOSE:
        error_at(p, "%s is not supported (there are no pointers): create objects with TClass.Create and free them with x.Free",
                 nm);
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
        if (!is_owning_type(a->type) && a->type->kind != T_OPT)
            error_at(p, "Take needs an optional place (an object or an optional value)");
        vpush(e->args, a);
        e->type = a->type;
        return e;
    }
    case BI_SETLENGTH: {
        NARGS(2, 2);
        Expr *s = value(A(0));
        check_writable(s, nm);
        if (s->type->kind != T_STR && s->type->kind != T_DYN) error_at(p, "string or 'array of' variable expected");
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

static int scope_has_frame(void) { return cur ? cur->has_frame : 0; }

/* 'opt is some k': declares k (inside the current scope) and unpacks into it */
static Expr *bind_some(Expr *e) {
    e->a = value(e->a);
    Type *t = e->a->type;
    Type *kt;
    if (e->op) { /* r is ok v / r is error e */
        const char *pat = e->op == 1 ? "ok" : "error";
        if (t->kind != T_RES) error_at(e->pos, "\"%s\" is not a result, 'is %s' needs a result value", type_name(t), pat);
        kt = e->op == 1 ? t->base : t->err;
        if (e->op == 1 && !kt && e->has_id)
            error_at(e->id.pos, "\"%s\" has no value: write 'is ok' without a name", type_name(t));
        if (e->op == 1 && kt && !e->has_id) error_at(e->pos, "name the value: 'is ok v'");
        e->type = t_bool;
        if (!e->has_id) {
            e->var = NULL;
            return e;
        }
    } else {
        if (!is_optional(t)) error_at(e->pos, "\"%s\" is not optional, 'is some' needs an optional value", type_name(t));
        kt = unwrapped(t);
        if (e->ro_bind && kt->kind == T_CLASS) kt = readonly_of(kt);
    }
    Var *k = new_var(e->id.name, kt, cur ? VK_LOCAL : VK_GLOBAL, e->id.pos);
    k->cname = sfmt("B%d_%s", k->id, e->id.name);
    k->is_global = !cur;
    k->level = level;
    k->in_frame = scope_has_frame();
    /* unpacking a place borrows its content; unpacking a fresh owned object takes ownership */
    k->borrowed = t->kind == T_CLASS && !is_owning_temp(e->a);
    if (t->kind == T_RES && kt && is_ref_type(kt)) k->borrowed = !res_temp(e->a); /* a fresh result gives its object away */
    if (t->kind == T_OPT && is_ref_type(kt)) k->borrowed = !is_owning_temp(e->a); /* optional T with T an object (generic body) */
    k->is_const = k->borrowed || t->kind == T_OPT || t->kind == T_RES;
    k->no_drop = (t->kind == T_OPT || t->kind == T_RES) && has_owned(kt) && !res_temp(e->a); /* a view: the original keeps owning;
                                                                                               from a fresh value k takes it over */
    if (cur) vpush(cur->locals, k);
    else vpush(prog->globals, k);
    add_sym(SY_VAR, e->id.name, e->id.pos)->var = k;
    e->var = k;
    e->type = t_bool;
    return e;
}

static Expr *cond(Expr *e);

/* condition that may unpack optionals: 'x is some k', joined with 'and' */
static Expr *cond_bind(Expr *e) {
    if (e->kind == E_ISSOME) return bind_some(e);
    if (e->kind == E_BINARY && e->op == OP_AND && (e->a->kind == E_ISSOME || e->b->kind == E_ISSOME ||
                                                   (e->a->kind == E_BINARY && e->a->op == OP_AND))) {
        e->a = cond_bind(e->a);
        e->b = cond_bind(e->b);
        e->type = t_bool;
        return e;
    }
    return cond(e);
}

static Expr *cond(Expr *e) {
    Expr *v = value(e);
    if (v->type->kind != T_BOOL) error_at(e->pos, "Boolean expression expected, got \"%s\"", type_name(v->type));
    return v;
}

static int has_call(Expr *e) {
    if (!e) return 0;
    if (e->kind == E_RCALL || e->kind == E_BUILTIN) return 1;
    if (has_call(e->a) || has_call(e->b)) return 1;
    for (int i = 0; i < e->args.len; i++)
        if (has_call(e->args.items[i])) return 1;
    return 0;
}

/* storing a value that owns objects moves it: the source place is emptied afterwards */
static void move_check(Expr *src, const char *ctx) {
    if (!src || !has_owned(src->type)) return;
    Expr *e = src;
    while (e->kind == E_CONV) e = e->a;
    if (e->kind == E_LIST || e->kind == E_RECLIT) {
        for (int i = 0; i < e->args.len; i++) {
            Expr *x = e->args.items[i];
            while (x->kind == E_CONV) x = x->a;
            if (has_owned(x->type) && is_lvalue(x))
                error_at(x->pos, "a value that owns objects cannot be copied into a literal; build it in place");
        }
        return;
    }
    if (!is_lvalue(e)) return; /* fresh value (function result, literal): nothing to empty */
    Var *rv = root_var(e);
    if (rooted_const(e) || (rv && (rv->no_drop || rv->is_const)))
        error_at(e->pos, "'%s' owns its contents (objects or heap memory) and cannot be copied (%s); move it from its owner or copy it with '.Clone'",
                 const_root_name(e), ctx);
    if (has_call(e)) error_at(e->pos, "moving a value that owns objects needs a plain place (no calls in it)");
}

static void assign(Stmt *s) {
    Expr *l = s->a;
    if (s->b->kind == E_TRY) try_site = s->b;
    if (l->kind == E_INDEX) {
        Expr *ib = NULL;
        Prop *ip = NULL;
        if (indexer_of(l, &ib, &ip)) { /* l[i] := v: the writer of the indexer */
            if (l->args.len != ip->idx.len)
                error_at(l->pos, "the indexer \"%s\" needs %d index%s", ip->name, ip->idx.len, ip->idx.len == 1 ? "" : "es");
            ExprVec a2 = {0};
            for (int i = 0; i < l->args.len; i++) vpush(a2, l->args.items[i]);
            vpush(a2, s->b);
            Routine *w = indexer_accessor(ib, ip, 1, l->pos);
            s->kind = S_CALL;
            s->a = make_call(w, ib, &a2, -1, NULL, l->pos);
            s->b = NULL;
            return;
        }
    }
    if (l->kind == E_IDENT && cur && cur->ret_ref && STREQ(l->id.name, "Result")) {
        Look R = lookup("Result");
        if (R.kind == LK_SYM && R.sym->kind == SY_VAR && R.sym->var == cur->result) { /* Result := Items[i]: refers to it */
            s->a = var_expr(cur->result, l->pos);
            if (s->b->kind == E_NIL) { /* none: no value */
                s->bind = 4;
                return;
            }
            Expr *r = value(s->b);
            if (!is_lvalue(r)) error_at(s->b->pos, "the 'var' result must refer to a variable or an element, not to a computed value");
            if (!var_compat(cur->result->type, r->type))
                error_at(s->b->pos, "the 'var' result (%s) cannot refer to \"%s\"", type_name(cur->result->type), type_name(r->type));
            s->b = r;
            s->bind = 1;
            return;
        }
    }
    if (l->kind == E_IDENT) {
        Look P = lookup(l->id.name);
        if (P.kind == LK_SYM && P.sym->kind == SY_VAR && P.sym->var->pending) { /* first assignment decides the type */
            Expr *r = value(s->b);
            settle_var(P.sym->var, infer_type(r, s->b->pos, l->id.name), r, l->pos);
            s->b = r;
        }
    }
    if (l->kind == E_IDENT) {
        Look B = lookup(l->id.name);
        if (B.kind == LK_SYM && B.sym->kind == SY_VAR && B.sym->var->is_box) {
            Var *v = B.sym->var;
            Expr *r = value(s->b);
            if (r->type->kind == T_PTR && r->type->base) { /* I := Integer.Create(...) */
                if (!same_type(r->type, v->type))
                    error_at(s->b->pos, "incompatible types: got \"%s\" expected \"%s\"", type_name(r->type), type_name(v->type));
                s->a = var_expr(v, l->pos);
                s->b = r;
                return;
            }
            Expr *d = new_expr(E_DEREF, l->pos); /* I := 5: store into the heap value */
            d->a = var_expr(v, l->pos);
            d->type = v->type->base;
            s->a = d;
            s->b = coerce(r, d->type, "assignment");
            return;
        }
    }
    if (l->kind == E_IDENT) {
        Look L = lookup(l->id.name);
        if (L.kind == LK_SYM && L.sym->kind == SY_ROUTINE) {
            for (Routine *q = cur; q; q = q->parent)
                if (q->result && in_overloads(L.sym->rt, q)) {
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
        if (L.kind == LK_MEMBER && L.p) {
            check_visible(&L, l->pos, l->id.name);
            base = var_expr(L.base, l->pos);
        }
        name = l->id.name;
    } else if (l->kind == E_MEMBER) {
        int is_type = 0;
        if (l->a->kind == E_IDENT) {
            Look T = lookup(l->a->id.name);
            is_type = T.kind == LK_SYM && T.sym->kind == SY_TYPE;
        }
        if (!is_type) {
            Expr *b = autoderef(value(l->a));
            if (b->type->kind == T_RECORD || b->type->kind == T_CLASS) {
                if (!find_member(b->type->st, l->id.name, &L.f, &L.m, &L.p) && find_cfield(b->type->st, l->id.name)) {
                    s->a = cfield_ref(find_cfield(b->type->st, l->id.name), l->pos);
                    goto normal;
                }
                if (!find_member(b->type->st, l->id.name, &L.f, &L.m, &L.p))
                    error_at(l->pos, "identifier \"%s\" is not a member of \"%s\"", l->id.name, b->type->st->name);
                check_visible(&L, l->pos, l->id.name);
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
    case S_ASSIGN:
        assign(s);
        if (s->kind == S_ASSIGN && !s->bind) move_check(s->b, "assignment"); /* a 'var' result refers, it does not move */
        if (s->kind == S_ASSIGN && s->b) no_method_value(s->b);
        if (s->kind == S_ASSIGN && cur && cur->owner && STREQ(cur->name, "GetEnumerator") && s->a->kind == E_FIELD &&
            s->a->field->borrowed && root_var(s->a) == cur->result)
            cur->enum_self = 1; /* Result.List := Self: the enumerator refers to its container */
        if (s->kind == S_ASSIGN && s->a->kind == E_VAR && s->a->type->kind == T_PROC && s->a->type->closure && s->a->var->level < level)
            error_at(s->a->pos, "a closure variable can only be assigned in the routine that declares it ('%s' belongs to an outer routine)",
                     s->a->var->name);
        return;
    case S_CALL: {
        Expr *c = s->a;
        if (c->kind == E_TRY) {
            try_site = c;
            s->a = ex(c); /* the value (if any) is dropped */
            return;
        }
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
                    move_check(s->a, "Exit");
                }
                return;
            }
        }
        Expr *e = ex(c);
        if (e->kind == E_METHREF)
            error_at(s->pos, "\"%s\" needs %d argument%s", e->rt->name, e->rt->params.len, e->rt->params.len == 1 ? "" : "s");
        if (e->type && e->type->kind == T_PROC && e->kind != E_RCALL && !e->type->proc->params.len) {
            ExprVec none = {0};
            e = indirect_call(e, &none, s->pos); /* 'f;' calls a procedure value without parameters */
        }
        if (e->kind != E_RCALL && e->kind != E_BUILTIN) error_at(s->pos, "illegal expression (statement expected)");
        s->a = e;
        return;
    }
    case S_COMPOUND: stmts(&s->body); return;
    case S_IF:
        if (s->a->kind == E_IS && s->a->has_args && s->a->args.len) { /* if s is Circle(r) then A else B: a case */
            Expr *is = s->a;
            Expr *sel = value(is->a);
            if (sel->type->kind == T_RECORD && sel->type->st->is_sum) {
                Expr *pat = new_expr(E_CALL, is->pos);
                pat->a = new_expr(E_IDENT, is->id.pos);
                pat->a->id = is->id;
                pat->args = is->args;
                CaseArm arm = {0};
                vpush(arm.lo, pat);
                vpush(arm.hi, NULL);
                arm.body = s->s1;
                Stmt *c = new_stmt(S_CASE, s->pos);
                c->a = sel;
                vpush(c->arms, arm);
                if (s->s2) {
                    c->has_else = 1;
                    vpush(c->else_, s->s2);
                }
                *s = *c;
                sum_case(s, 1);
                return;
            }
            is->a = sel;
        }
        push_scope(); /* bindings of 'is some' are visible in the then-branch */
        s->a = cond_bind(s->a);
        stmt(s->s1);
        pop_scope();
        if (s->s2) stmt(s->s2);
        return;
    case S_FORIN: for_in(s); return;
    case S_WHILE:
        push_scope();
        if (s->a) s->a = cond_bind(s->a); /* no condition: 'loop' */
        loop_depth++;
        stmt(s->s1);
        loop_depth--;
        pop_scope();
        return;
    case S_REPEAT:
        loop_depth++;
        stmts(&s->body);
        loop_depth--;
        s->a = cond(s->a);
        return;
    case S_FOR: {
        if (s->decl_var) { /* for val i := a to b: the loop declares its counter */
            push_scope();
            s->a = value(s->a);
            Type *ft = infer_type(s->a, s->a->pos, s->var.name);
            if (!is_ordinal(ft)) error_at(s->var.pos, "loop variable must be of an ordinal type");
            if (ft->kind == T_INT && ft->ik != IK_I64) ft = t_int;
            Var *nv = new_var(s->var.name, ft, cur ? VK_LOCAL : VK_GLOBAL, s->var.pos);
            nv->cname = sfmt("F%d_%s", nv->id, s->var.name);
            nv->in_frame = cur ? cur->has_frame : 0;
            if (cur) vpush(cur->locals, nv);
            else vpush(prog->globals, nv);
            add_sym(SY_VAR, s->var.name, s->var.pos)->var = nv;
            s->decl_var = 0;
            stmt(s);
            pop_scope();
            return;
        }
        Look L = lookup(s->var.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_VAR)
            error_at(s->var.pos, "loop variable \"%s\" must be a simple variable", s->var.name);
        Var *v = L.sym->var;
        if (v->pending) { /* for i := 1 to n: the start value decides the type of i */
            if (v->decl_heap) error_at(s->var.pos, "loop variable \"%s\" must be declared in 'val', not in 'var'", v->name);
            s->a = value(s->a);
            settle_var(v, infer_type(s->a, s->a->pos, v->name), s->a, s->var.pos);
        }
        if (v->is_box) error_at(s->var.pos, "loop variable \"%s\" must be declared in 'val', not in 'var'", v->name);
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
        if (t->kind == T_RECORD && t->st->is_sum) {
            sum_case(s, 0);
            return;
        }
        if (t->kind == T_RES) {
            /* case r of ok v: A; error e: B end  ==>  tmp := r; if tmp is ok v then A else if tmp is error e then B */
            Expr *okp = NULL, *errp = NULL;
            Stmt *ok_body = NULL, *err_body = NULL;
            for (int i = 0; i < s->arms.len; i++) {
                CaseArm *arm = &s->arms.items[i];
                Expr *l = arm->lo.len == 1 && !arm->hi.items[0] ? arm->lo.items[0] : NULL;
                if (!l || l->kind != E_SOMEPAT || l->op == 0 || (l->op == 1 && okp) || (l->op == 2 && errp))
                    error_at(arm->body->pos, "result case arms are 'ok v:' and 'error e:' (each at most once)");
                if (l->op == 1) { okp = l; ok_body = arm->body; }
                else { errp = l; err_body = arm->body; }
            }
            if (s->has_else) {
                if (okp && errp) error_at(s->pos, "'else' is unreachable: both 'ok' and 'error' are handled");
                Stmt *c = new_stmt(S_COMPOUND, s->pos);
                c->body = s->else_;
                if (!okp) ok_body = c;
                else err_body = c;
            }
            /* the selector is evaluated once, into a hidden variable */
            Var *tv = new_var(sfmt("__case%d", s->pos.line), t, cur ? VK_LOCAL : VK_GLOBAL, s->pos);
            tv->cname = sfmt("C%d_res", tv->id);
            tv->is_global = !cur;
            tv->level = level;
            tv->in_frame = scope_has_frame();
            if (cur) vpush(cur->locals, tv);
            else vpush(prog->globals, tv);
            Stmt *as = new_stmt(S_ASSIGN, s->pos);
            as->a = var_expr(tv, s->pos);
            as->b = s->a;
            Expr *iso = new_expr(E_ISSOME, s->pos);
            iso->op = 1;
            iso->a = var_expr(tv, s->pos);
            if (okp && okp->has_id) { iso->id = okp->id; iso->has_id = 1; }
            else if (t->base) { iso->id = (Ident){sfmt("__ok%d", s->pos.line), s->pos}; iso->has_id = 1; }
            Stmt *n = new_stmt(S_IF, s->pos);
            n->a = iso;
            n->s1 = ok_body ? ok_body : new_stmt(S_EMPTY, s->pos);
            if (err_body) {
                if (errp) {
                    Expr *ise = new_expr(E_ISSOME, s->pos);
                    ise->op = 2;
                    ise->a = var_expr(tv, s->pos);
                    ise->id = errp->id;
                    ise->has_id = 1;
                    Stmt *n2 = new_stmt(S_IF, s->pos);
                    n2->a = ise;
                    n2->s1 = err_body;
                    n->s2 = n2;
                } else
                    n->s2 = err_body;
            }
            if (has_owned(t)) { /* owns an object: a hidden variable of a block, freed when the case ends */
                char *hn = sfmt("__case%d_%d", s->pos.line, s->pos.col);
                Block *ob = NEW(Block);
                Decl *d = NEW(Decl);
                d->kind = D_VAR;
                VarDecl hv = {0};
                vpush(hv.names, ((Ident){hn, s->pos}));
                hv.init = s->a;
                vpush(d->vars, hv);
                vpush(ob->decls, d);
                iso->a = mk_ident(hn, s->pos);
                if (n->s2 && n->s2->kind == S_IF && n->s2->a->kind == E_ISSOME) n->s2->a->a = mk_ident(hn, s->pos);
                vpush(ob->body, n);
                Stmt *blk = new_stmt(S_BLOCK, s->pos);
                blk->blk = ob;
                *s = *blk;
                stmt(s);
                return;
            }
            Stmt *c = new_stmt(S_COMPOUND, s->pos);
            vpush(c->body, as);
            vpush(c->body, n);
            *s = *c;
            stmt(s);
            return;
        }
        if (is_optional(t)) {
            /* case opt of some x: ...; none: ... end  ==>  if opt is some x then ... else ... */
            Stmt *some_body = NULL, *none_body = NULL;
            Expr *pat = NULL;
            for (int i = 0; i < s->arms.len; i++) {
                CaseArm *arm = &s->arms.items[i];
                if (arm->lo.len != 1 || arm->hi.items[0]) error_at(arm->body->pos, "optional case arms are 'some x:' or 'none:'");
                Expr *l = arm->lo.items[0];
                if (l->kind == E_SOMEPAT && !some_body) {
                    pat = l;
                    some_body = arm->body;
                } else if (l->kind == E_NIL && !none_body)
                    none_body = arm->body;
                else
                    error_at(l->pos, "optional case arms are 'some x:' and 'none:' (each at most once)");
            }
            if (s->has_else) {
                if (none_body && some_body) error_at(s->pos, "'else' is unreachable: both 'some' and 'none' are handled");
                Stmt *c = new_stmt(S_COMPOUND, s->pos);
                c->body = s->else_;
                if (!none_body) none_body = c;
                else if (!some_body) some_body = c;
            }
            Stmt *n = new_stmt(S_IF, s->pos);
            if (pat) {
                Expr *is = new_expr(E_ISSOME, pat->pos);
                is->a = s->a;
                is->id = pat->id;
                n->a = is;
                n->s1 = some_body ? some_body : new_stmt(S_EMPTY, s->pos);
                n->s2 = none_body;
            } else {
                Expr *eq = new_expr(E_BINARY, s->pos);
                eq->op = OP_EQ;
                eq->a = s->a;
                eq->b = new_expr(E_NIL, s->pos);
                n->a = eq;
                n->s1 = none_body ? none_body : new_stmt(S_EMPTY, s->pos);
                n->s2 = some_body;
            }
            *s = *n;
            stmt(s);
            return;
        }
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
    case S_EXIT: return;
    case S_BLOCK: {
        /* begin-block with its own declarations: a new scope */
        for (int i = 0; i < s->blk->decls.len; i++) {
            DeclKind k = s->blk->decls.items[i]->kind;
            if (k == D_ROUTINE || k == D_TYPE)
                error_at(s->pos, "only const, val and var sections are allowed before a nested begin ... end");
        }
        push_scope();
        StmtVec *save_init = local_init;
        StmtVec inits = {0};
        local_init = &inits;
        in_block++;
        int n0 = cur ? cur->locals.len : prog->globals.len;
        decls(s->blk, cur ? cur->has_frame : 0);
        VEC(Var *) *all = cur ? (void *)&cur->locals : (void *)&prog->globals;
        for (int i = n0; i < all->len; i++) vpush(s->wvars, all->items[i]);
        in_block--;
        local_init = save_init;
        stmts(&s->blk->body);
        StmtVec body = {0};
        for (int i = 0; i < inits.len; i++) vpush(body, inits.items[i]);
        for (int i = 0; i < s->blk->body.len; i++) vpush(body, s->blk->body.items[i]);
        s->blk->body = body;
        check_settled(s->wvars.items, s->wvars.len);
        pop_scope();
        return;
    }
    }
}

/* ---- error recovery: after an error the statement/declaration is skipped and checking goes on ---- */
#define RECOVER_BEGIN                                                                                                  \
    {                                                                                                                  \
        SemaState rec_st = save_state();                                                                               \
        jmp_buf rec_jb, *rec_prev = g_recover;                                                                         \
        g_recover = &rec_jb;                                                                                           \
        if (setjmp(rec_jb) == 0) {
#define RECOVER_FAIL(code)                                                                                             \
        } else {                                                                                                       \
            restore_state(&rec_st);                                                                                    \
            code;                                                                                                      \
        }                                                                                                              \
        g_recover = rec_prev;                                                                                          \
    }
#define RECOVER_END                                                                                                    \
        } else                                                                                                         \
            restore_state(&rec_st);                                                                                    \
        g_recover = rec_prev;                                                                                          \
    }
typedef struct {
    Scope *scope;
    Routine *cur;
    int level, loop_depth, in_block, generic_depth;
    StmtVec *local_init;
    const char *err_context, *inst_cname;
    Struct *inst_cls;
    Expr *try_site;
} SemaState;
static SemaState save_state(void) {
    SemaState st = {scope, cur, level, loop_depth, in_block, generic_depth, local_init, g_err_context, inst_cname, inst_cls, try_site};
    return st;
}
static void restore_state(SemaState *st) {
    scope = st->scope;
    cur = st->cur;
    level = st->level;
    loop_depth = st->loop_depth;
    in_block = st->in_block;
    generic_depth = st->generic_depth;
    local_init = st->local_init;
    g_err_context = st->err_context;
    inst_cname = st->inst_cname;
    inst_cls = st->inst_cls;
    try_site = st->try_site;
}

static void stmts(StmtVec *v) {
    for (int i = 0; i < v->len; i++) {
        RECOVER_BEGIN
        stmt(v->items[i]);
        RECOVER_END
    }
}

/* ============================================================== declarations */

static void routine_body(Routine *rt, RoutineDecl *d);

static void add_init(StmtVec *dst, Var *v, Expr *init) {
    Stmt *s = new_stmt(S_ASSIGN, init->pos);
    Expr *r = value(init);
    if (v->is_box && !(r->type->kind == T_PTR && r->type->base)) { /* heap value: store into the (new) cell */
        Expr *d = new_expr(E_DEREF, init->pos);
        d->a = var_expr(v, init->pos);
        d->type = v->type->base;
        s->a = d;
        s->b = coerce(r, d->type, "initializer");
    } else {
        s->a = var_expr(v, init->pos);
        s->b = coerce(r, v->type, "initializer");
    }
    no_method_value(s->b);
    move_check(s->b, "initializer");
    vpush(*dst, s);
}


/* type of a variable declared without ': Type', taken from the value assigned to it */
static Type *infer_type(Expr *r, Pos p, const char *name) {
    if (r->kind == E_TYPEREF || r->type == t_void) error_at(p, "cannot infer the type of '%s' from this expression", name);
    if (r->type->kind == T_NIL) error_at(p, "cannot infer the type of '%s' from none: declare it as '%s: optional T'", name, name);
    if (r->kind == E_SET && r->type->kind == T_LIT && r->args.len) return set_literal(r, set_of_elems(r))->type;
    if (r->type->kind == T_LIT)
        error_at(p, "cannot infer the type of '%s' from a literal or 'default': declare the type ('%s: T := ...')", name, name);
    if (r->type->kind == T_PTR && r->type->base && r->kind == E_BUILTIN && r->bi == BI_BOXNEW) return r->type->base;
    return r->type;
}

/* give a variable declared without a type its type */
static void settle_var(Var *v, Type *t, Expr *src, Pos p) {
    v->pending = 0;
    if (v->decl_heap) {
        if (t->kind == T_CLASS) {
            v->type = t; /* owner of a heap object */
        } else {
            Type *b = ptr_to(t);
            b->name = sfmt("heap %s", type_name(t));
            v->type = b;
            v->is_box = 1;
        }
        return;
    }
    if (t->kind == T_CLASS) {
        if (src && is_owning_temp(src))
            error_at(p, "'%s' would be a stack object: declare it with a type ('val %s: %s;' and %s.Create(...)), or use 'var %s = ...' for a heap object",
                     v->name, v->name, type_name(t), v->name, v->name);
        v->borrowed = 1; /* val x = someObject: a borrowed reference */
    }
    v->type = t;
}

static void check_settled(Var **vars, int n) {
    for (int i = 0; i < n; i++)
        if (vars[i]->pending)
            error_at(vars[i]->pos, "the type of '%s' cannot be determined: it is never assigned (declare '%s: Type')",
                     vars[i]->name, vars[i]->name);
}

static void routine_decl(RoutineDecl *d) {
    Routine *rt = NULL;
    if (d->has_cls && d->tparams.len) { /* function TA.Id<T>(x: T): T: instances are made from it when it is used */
        if (d->cls_tparams.len) error_at(d->pos, "generic methods of generic types are not supported (yet)");
        if (cur) error_at(d->pos, "method implementations must be at program level");
        Look L = lookup(d->cls.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_TYPE || (L.sym->type->kind != T_CLASS && L.sym->type->kind != T_RECORD))
            error_at(d->cls.pos, "\"%s\" is not a class or record type", d->cls.name);
        Struct *st = L.sym->type->st;
        Generic *g = NULL;
        for (int i = 0; i < st->gmethods.len; i++)
            if (STREQ(((Generic *)st->gmethods.items[i])->name, d->name.name)) g = st->gmethods.items[i];
        if (!g) error_at(d->name.pos, "\"%s\" has no generic method \"%s\"", st->name, d->name.name);
        if (g->params.len != d->tparams.len)
            error_at(d->pos, "\"%s.%s\" has %d type parameter(s) in its declaration", st->name, g->name, g->params.len);
        if (g->toks && g->toks != d->toks) error_at(d->pos, "\"%s.%s\" is already implemented", st->name, g->name);
        g->toks = d->toks;
        g->tok = d->tok_start;
        g->scope = scope;
        return;
    }
    if (d->tparams.len) { /* function Max<T>(...): kept, made for every type combination it is called with */
        if (cur) error_at(d->pos, "generic routines must be declared at program level");
        if (d->forward && !d->iface) error_at(d->pos, "generic routines cannot be forward declared");
        if (iface_scope) { /* definition of a generic routine declared in the module interface */
            Sym *is = find_in(iface_scope, d->name.name);
            if (is && is->kind == SY_GENERIC && is->gen->iface_only) {
                Generic *g = is->gen;
                if (g->params.len != d->tparams.len)
                    error_at(d->pos, "'%s' has %d type parameter(s) in the interface", d->name.name, g->params.len);
                g->toks = d->toks; /* instances are made from the definition */
                g->tok = d->tok_start;
                g->rdecl = d;
                g->scope = scope;  /* private helpers of the implementation are visible */
                g->iface_only = 0;
                return;
            }
        }
        Generic *g = NEW(Generic);
        g->name = d->name.name;
        g->params = d->tparams;
        g->toks = d->toks;
        g->tok = d->tok_start;
        g->pos = d->name.pos;
        g->is_routine = 1;
        g->iface_only = d->iface;
        g->rdecl = d;
        g->cons = d->tcons;
        g->scope = scope;
        validate_constraints(g);
        vpush(all_generics, g);
        add_sym(SY_GENERIC, d->name.name, d->name.pos)->gen = g;
        return;
    }
    if (d->has_cls && d->cls_tparams.len && !inst_cls) { /* procedure TListe<T>.Add: kept for every instance */
        if (cur) error_at(d->pos, "method implementations must be at program level");
        Look L = lookup(d->cls.name);
        if (L.kind != LK_SYM || L.sym->kind != SY_GENERIC) error_at(d->cls.pos, "\"%s\" is not a generic type", d->cls.name);
        Generic *g = L.sym->gen;
        if (d->cls_tparams.len != g->params.len)
            error_at(d->cls.pos, "\"%s\" has %d type parameter%s", g->name, g->params.len, g->params.len == 1 ? "" : "s");
        vpush(g->impls, d);
        for (int i = 0; i < g->insts.len; i++) { /* instances made before this implementation was seen */
            PendingBody pb = {g, g->insts.items[i], d, NULL, NULL, NULL};
            vpush(pending_bodies, pb);
        }
        return;
    }
    if (d->has_cls) {
        if (cur) error_at(d->pos, "method implementations must be at program level");
        Struct *st;
        if (inst_cls) st = inst_cls;
        else {
            Look L = lookup(d->cls.name);
            if (L.kind == LK_SYM && L.sym->kind == SY_GENERIC)
                error_at(d->cls.pos, "\"%s\" is generic: write 'procedure %s<...>.%s'", d->cls.name, d->cls.name, d->name.name);
            if (L.kind != LK_SYM || L.sym->kind != SY_TYPE ||
                (L.sym->type->kind != T_CLASS && L.sym->type->kind != T_RECORD))
                error_at(d->cls.pos, "\"%s\" is not a class or record type", d->cls.name);
            st = L.sym->type->st;
        }
        int ncand = 0;
        for (int i = 0; i < st->methods.len; i++)
            if (STREQ(st->methods.items[i]->name, d->name.name)) rt = st->methods.items[i], ncand++;
        if (!rt) error_at(d->name.pos, "method \"%s\" is not declared in \"%s\"", d->name.name, st->name);
        if (ncand > 1) { /* overloaded: the declaration with the same parameters */
            Routine tmp = {0};
            tmp.name = rt->name;
            build_params(&tmp, &d->params);
            rt = NULL;
            for (int i = 0; i < st->methods.len && !rt; i++) {
                Routine *m = st->methods.items[i];
                if (STREQ(m->name, d->name.name) && same_params(m, &tmp)) rt = m;
            }
            if (!rt) error_at(d->name.pos, "\"%s.%s\" is not declared with these parameters", st->name, sig_text(&tmp));
        }
        if (rt->kind != d->kind) error_at(d->pos, "method kind does not match declaration of \"%s\"", rt->name);
        if (rt->is_classm != d->is_classm)
            error_at(d->pos, "\"%s.%s\" is declared %sas class method: the implementation must match ('%s')", st->name,
                     rt->name, rt->is_classm ? "" : "not ",
                     rt->is_classm ? (rt->kind == RK_FUNC ? "class function ..." : "class procedure ...")
                                   : (rt->kind == RK_FUNC ? "function ..." : "procedure ..."));
        if (rt->is_constm != d->is_const)
            error_at(d->pos, "\"%s.%s\" is declared %s'const': the implementation must match", st->name, rt->name,
                     rt->is_constm ? "" : "without ");
        if (rt->is_auto) error_at(d->pos, "\"%s.%s\" is generated by the compiler ('auto'): remove this implementation", st->name, rt->name);
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
    if (d->is_classm) error_at(d->pos, "'class' is only allowed for methods ('class function TFoo.%s')", d->name.name);
    Sym *ex_sym = find_local(d->name.name);
    if (!ex_sym && iface_scope) ex_sym = find_in(iface_scope, d->name.name); /* defines an interface header */
    Routine *ovl_prev = NULL; /* a new overload is added after this one */
    if (ex_sym && ex_sym->kind == SY_ROUTINE && (d->is_overload || ex_sym->rt->ovl_next)) {
        Routine tmp = {0};
        tmp.name = ex_sym->rt->name;
        build_params(&tmp, &d->params);
        Routine *match = NULL;
        for (Routine *o = ex_sym->rt; o; o = o->ovl_next) {
            if (same_params(o, &tmp)) match = o;
            ovl_prev = o;
        }
        if (!(match && match->forward && !match->defined)) /* not the definition of a header: a new overload */
            for (Routine *o = ex_sym->rt; o; o = o->ovl_next)
                if (!o->is_overload || !d->is_overload)
                    error_at(d->pos, "duplicate routine \"%s\" (to give several routines the same name, mark each of them with 'overload')",
                             d->name.name);
        if (match && !(match->forward && !match->defined))
            error_at(d->pos, "\"%s\" is overloaded twice with the same parameters", sig_text(&tmp));
        if (match) { /* the definition of a forward or interface header */
            ex_sym = NULL;
            rt = match;
            if (d->forward) error_at(d->pos, "duplicate forward declaration");
            if (d->kind == RK_FUNC && d->ret && !same_type(resolve_type(d->ret), rt->ret))
                error_at(d->pos, "declaration of \"%s\" differs from forward declaration", rt->name);
            for (int i = 0; i < rt->params.len; i++) rt->params.items[i].name = tmp.params.items[i].name;
            ovl_prev = NULL;
        } else
            ex_sym = NULL; /* a new overload */
    }
    if (!rt && ex_sym && ex_sym->kind == SY_ROUTINE && !(ex_sym->rt->forward && !ex_sym->rt->defined))
        error_at(d->pos, "duplicate routine \"%s\" (to give several routines the same name, mark each of them with 'overload')", d->name.name);
    if (rt) {
    } else if (ex_sym && ex_sym->kind == SY_ROUTINE && ex_sym->rt->forward && !ex_sym->rt->defined) {
        rt = ex_sym->rt;
        if (d->forward) error_at(d->pos, "duplicate forward declaration");
        if (d->has_params) {
            Routine tmp = *rt;
            tmp.params.items = NULL;
            tmp.params.len = tmp.params.cap = 0;
            build_params(&tmp, &d->params);
            if (d->ret) tmp.ret = resolve_type(d->ret);
            if (!same_signature(rt, &tmp)) error_at(d->pos, "declaration of \"%s\" differs from forward declaration", rt->name);
            if (rt->is_constm != d->is_const) error_at(d->pos, "'const' of \"%s\" differs from its forward declaration", rt->name);
            for (int i = 0; i < rt->params.len; i++) rt->params.items[i].name = tmp.params.items[i].name;
        }
    } else {
        if (d->kind == RK_CTOR || d->kind == RK_DTOR)
            error_at(d->pos, "constructors/destructors must be methods");
        rt = new_routine(d->name.name, d->kind, d->name.pos);
        rt->is_constm = d->is_const;
        rt->level = level + 1;
        rt->parent = cur;
        rt->cname = inst_cname ? xstrdup(inst_cname) : cur ? sfmt("%s_%s", cur->cname, d->name.name) : sfmt("R_%s", d->name.name);
        build_params(rt, &d->params);
        if (d->kind == RK_FUNC) {
            if (!d->ret) error_at(d->pos, "function result type expected");
            rt->ret = resolve_type(d->ret);
            no_closure(rt->ret, d->ret->pos, "the result of a function");
        }
        rt->ret_borrowed = d->ret_borrowed;
        if (d->ret_borrowed && !is_owning_type(rt->ret)) {
            if (has_owned(rt->ret)) rt->ret_ref = 1;
            else {
                if (!generic_depth) error_at(d->pos, "a 'var' result (reference without ownership) needs a class type or a value that owns objects");
                rt->ret_borrowed = 0;
            }
        }
        rt->is_overload = d->is_overload;
        if (ovl_prev) { /* another overload: it shares the symbol of the first one */
            ovl_prev->ovl_next = rt;
            int n = 1;
            for (Routine *o = find_routine_sym(d->name.name); o && o != rt; o = o->ovl_next) n++;
            rt->cname = sfmt("%s_%d", rt->cname, n);
        } else {
            Sym *s = add_sym(SY_ROUTINE, d->name.name, d->name.pos);
            s->rt = rt;
        }
    }
    if (d->external) {
        if (rt->level != 1) error_at(d->pos, "external routines must be declared at the top level");
        rt->cname = d->external;
        rt->is_external = 1;
        rt->defined = 1;
        return;
    }
    if (d->forward) {
        rt->forward = 1;
        return;
    }
    routine_body(rt, d);
}

static void routine_body(Routine *rt, RoutineDecl *d) {
    Scope *save_top = top_scope;
    if (rt->level == 1) top_scope = scope; /* lambdas inside see what this routine sees from outside */
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
    if (d->has_lambda) has_frame = 1; /* its lambdas may capture its variables */
    rt->has_frame = has_frame;

    if (rt->owner && rt->is_classm) push_scope()->cls = rt->owner;
    else if (rt->owner) {
        Var *self = new_var("Self", rt->owner->type, VK_SELF, rt->pos);
        self->is_ref = rt->owner->kind == SK_RECORD; /* record methods work on the caller's record */
        self->borrowed = 1;
        self->is_const = rt->is_constm; /* const method: Self is read-only */
        if (rt->is_constm && rt->owner->kind == SK_CLASS) self->type = readonly_of(self->type);
        self->in_frame = has_frame;
        rt->self = self;
        push_scope()->self_obj = self;
    }
    push_scope();
    if (rt->self) add_sym(SY_VAR, "Self", rt->pos)->var = rt->self;
    for (int i = 0; i < rt->params.len; i++) {
        Param *pm = &rt->params.items[i];
        Var *v = new_var(pm->name, pm->type, VK_PARAM, d->pos);
        v->mode = pm->mode;
        v->is_ref = pm->mode == PM_VAR || pm->mode == PM_OUT || pm->mode == PM_CONSTREF;
        v->is_const = pm->mode == PM_CONST || pm->mode == PM_CONSTREF;
        v->in_frame = has_frame;
        v->borrowed = (pm->mode == PM_VALUE || pm->mode == PM_CONST || pm->mode == PM_CONSTREF) && is_owning_type(pm->type);
        pm->var = v;
        add_sym(SY_VAR, pm->name, d->pos)->var = v;
    }
    if (rt->kind == RK_FUNC) {
        Var *r = new_var("Result", rt->ret, VK_RESULT, d->pos);
        r->borrowed = rt->ret_borrowed;
        r->is_ref = rt->ret_ref; /* a pointer to the value it refers to */
        r->no_drop = rt->ret_ref;
        r->in_frame = has_frame;
        rt->result = r;
        if (!find_local("Result")) add_sym(SY_VAR, "Result", d->pos)->var = r;
    }
    decls(d->body, has_frame);
    stmts(&d->body->body);
    check_settled(rt->locals.items, rt->locals.len);
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
    top_scope = save_top;
}

static void decls(Block *b, int has_frame) {
    for (int di = 0; di < b->decls.len; di++) /* implementations of generic methods: needed when they are used */
        if (b->decls.items[di]->kind == D_ROUTINE && b->decls.items[di]->rt->has_cls && b->decls.items[di]->rt->tparams.len)
            vpush(gm_impls, b->decls.items[di]->rt);
    for (int di = 0; di < b->decls.len; di++) {
        RECOVER_BEGIN
        decl1(b->decls.items[di], has_frame);
        RECOVER_END
    }
    decls_tail(b);
}

static void decl1(Decl *d, int has_frame) {
    {
        switch (d->kind) {
        case D_CONST:
            for (int i = 0; i < d->consts.len; i++) {
                ConstDecl *c = &d->consts.items[i];
                {
                    Expr *r = c->value->kind == E_LIST ? NULL : value(c->value);
                    Expr *lit = r ? fold(r) : NULL;
                    if (r && !lit)
                        error_at(c->name.pos, "'%s' is not a compile-time constant: for a value computed at run time write 'val %s: %s const := ...'",
                                 c->name.name, c->name.name, type_name(r->type));
                    if (!c->ty && !r) error_at(c->name.pos, "an initializer list needs a declared array type");
                    if (lit && !c->ty) { /* compile-time constant */
                        Sym *cs = add_sym(SY_CONST, c->name.name, c->name.pos);
                        cs->cval = lit;
                        cs->type = lit->type;
                        continue;
                    }
                }
                {
                    /* typed constant = initialized, read-only static variable */
                    Type *t = resolve_type(c->ty);
                    int save = level;
                    level = 0;
                    Var *v = new_var(c->name.name, t, VK_GLOBAL, c->name.pos);
                    level = save;
                    v->is_global = 1;
                    v->level = 0;
                    vpush(prog->globals, v);
                    add_sym(SY_VAR, c->name.name, c->name.pos)->var = v;
                    add_init(&prog->init, v, c->value);
                    v->is_const = 1;
                }
            }
            break;
        case D_TYPE: {
            /* generic types are only remembered; every instance is made on first use */
            for (int i = 0; i < d->types.len; i++) {
                TypeDecl *td = &d->types.items[i];
                if (!td->tparams.len) continue;
                if (cur || in_block) error_at(td->name.pos, "generic types must be declared at program level");
                if (td->ty->kind != TE_STRUCT && td->ty->kind != TE_RECORD)
                    error_at(td->name.pos, "only records and classes can be generic");
                Generic *g = NEW(Generic);
                g->name = td->name.name;
                g->params = td->tparams;
                g->toks = td->toks;
                g->tok = td->tok_start;
                g->pos = td->name.pos;
                g->cons = td->tcons;
                g->scope = scope;
                vpush(all_generics, g);
                add_sym(SY_GENERIC, td->name.name, td->name.pos)->gen = g;
            }
            /* pass 1: pre-register records/objects/classes for forward references */
            for (int i = 0; i < d->types.len; i++) {
                TypeDecl *td = &d->types.items[i];
                if (td->tparams.len) continue;
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
                if (td->tparams.len) continue;
                TEKind k = td->ty->kind;
                if (k == TE_CLASSFWD) continue;
                if (k == TE_STRUCT || k == TE_RECORD) {
                    Struct *st = find_local(td->name.name)->type->st;
                    if (k == TE_RECORD) define_record(st, td->ty);
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
            for (int i = 0; i < d->types.len; i++) /* constraints may name classes of this section */
                if (d->types.items[i].tparams.len) validate_constraints(find_local(d->types.items[i].name.name)->gen);
            for (int i = 0; i < d->types.len; i++) {
                if (d->types.items[i].tparams.len) continue;
                Sym *s = find_local(d->types.items[i].name.name);
                if (s && s->kind == SY_TYPE && s->type->st && !s->type->st->defined)
                    error_at(d->types.items[i].name.pos, "forward class \"%s\" is never defined", s->name);
            }
            break;
        }
        case D_VAR:
            for (int i = 0; i < d->vars.len; i++) {
                VarDecl *vd = &d->vars.items[i];
                RECOVER_BEGIN
                Pos dpos = vd->names.items[0].pos;
                const char *vn = vd->names.items[0].name, *sec = vd->is_heap ? "vao" : vd->borrowed ? "var" : "val";
                if (!vd->init) {
                    /* every variable is initialized in its declaration */
                    if (!vd->ty) error_at(dpos, "'%s' must be initialized: '%s %s := ...'", vn, sec, vn);
                    Type *t0 = resolve_type(vd->ty);
                    if (t0->kind == T_CLASS && !vd->is_heap && !vd->borrowed)
                        error_at(dpos, "a stack object is created in its declaration: 'val %s := %s.Create(...)' (or 'vao' for a heap object, 'var' for a reference)",
                                 vn, type_name(t0));
                    if (t0->kind == T_CLASS && vd->is_heap && !t0->nilable)
                        error_at(dpos, "'%s' must be initialized: 'vao %s := %s.Create(...)' (or 'vao %s: optional %s := none')",
                                 vn, vn, type_name(t0), vn, type_name(t0));
                    if (t0->kind == T_CLASS)
                        error_at(dpos, "'%s' must be initialized: '%s %s: %s := %s'", vn, sec, vn, type_name(t0),
                                 t0->nilable ? "none" : "...");
                    if (vd->borrowed)
                        error_at(dpos, "'%s' refers to another variable and must be initialized with it: 'var %s: %s := other'",
                                 vn, vn, type_name(t0));
                    error_at(dpos, "'%s' must be initialized: '%s %s: %s := ...' (':= default' gives the zero value)", vn, sec,
                             vn, type_name(t0));
                }
                Type *t;
                Expr *r = NULL;
                if (vd->ty) {
                    t = resolve_type(vd->ty);
                    check_complete(t, vd->ty->pos);
                } else {
                    if (vd->init->kind == E_TRY) try_site = vd->init;
                    r = value(vd->init);
                    t = infer_type(r, vd->init->pos, vn);
                }
                /* val k := TFoo.Create(...): a stack object, constructed in place, destroyed at the end of its block */
                if (!cur) no_closure(t, dpos, "a variable of the main program");
                int is_stack = t->kind == T_CLASS && !vd->is_heap && !vd->borrowed;
                if (is_stack && generic_depth && vd->ty && t->nilable) is_stack = 0; /* generic code: 'val h: optional T' owns a reference */
                if (is_stack && r && r->kind == E_RCALL && r->cmode == CM_NEWCLASS && r->b)
                    error_at(dpos, "an object made through a class reference gets its class only at run time, so it cannot be a stack object: 'vao %s := ...'", vn);
                Expr *ctor = NULL;
                if (is_stack) {
                    if (t->nilable && vd->ty)
                        error_at(dpos, "a stack object always exists, it cannot be optional: use 'vao %s: %s := none' for a heap object that may be missing",
                                 vn, type_name(t));
                    if (!r) r = value(vd->init);
                    if (r->kind == E_VAR && r->var->is_stack) { /* val b := k: the object moves into b */
                        if (vd->ty && r->var->type->st != t->st)
                            error_at(vd->init->pos, "stack object '%s' is declared as \"%s\" but '%s' is a \"%s\": both must be the same class",
                                     vn, type_name(t), r->var->name, type_name(r->var->type));
                        if (!vd->ty) t = r->var->type;
                    } else if (!(r->kind == E_RCALL && r->cmode == CM_NEWCLASS) && r->type->kind == T_CLASS) {
                        /* val k := h: a heap object moves into the stack frame (its memory is given back) */
                        if (r->type->nilable)
                            error_at(vd->init->pos, "the object for stack object '%s' may be none (\"%s\"): unpack it first with 'is some', or use 'vao'",
                                     vn, type_name(r->type));
                        if (vd->ty && r->type->st != t->st)
                            error_at(vd->init->pos, "stack object '%s' is declared as \"%s\" but the object is a \"%s\": both must be the same class",
                                     vn, type_name(t), type_name(r->type));
                        if (!vd->ty) t = r->type->readonly ? r->type->st->type : r->type;
                        if (r->type->readonly) error_at(vd->init->pos, "a read-only reference cannot be moved into a stack object");
                    } else if (r->kind != E_RCALL || r->cmode != CM_NEWCLASS)
                        error_at(vd->init->pos, "a stack object is created in its declaration: 'val %s := %s.Create(...)' (for a reference use 'var', to own a heap object 'vao')",
                                 vn, type_name(t));
                    int is_new = r->kind == E_RCALL && r->cmode == CM_NEWCLASS;
                    if (is_new && r->b)
                        error_at(dpos, "an object made through a class reference gets its class only at run time, so it cannot be a stack object: 'vao %s := ...'", vn);
                    if (is_new && vd->ty && r->ctype->st != t->st)
                        error_at(vd->init->pos, "stack object '%s' is declared as \"%s\" but created as \"%s\": it is stored in place, so both must be the same class",
                                 vn, type_name(t), type_name(r->ctype));
                    if (is_new && !vd->ty) t = r->ctype;
                    ctor = r;
                }
                if (vd->ro && t->kind == T_CLASS) t = readonly_of(t); /* for val x in ...: read-only */
                int is_alias = vd->borrowed && t->kind != T_CLASS; /* var x: Integer := y: x refers to y */
                int is_const = (t->readonly || vd->ro) && t->kind != T_CLASS;
                int is_box = 0;
                if (vd->is_heap && t->kind != T_CLASS) { /* vao I := 5: an owned value on the heap */
                    Type *bx = ptr_to(t);
                    bx->name = sfmt("heap %s", type_name(t));
                    t = bx;
                    is_box = 1;
                }
                for (int k = 0; k < vd->names.len; k++) {
                    Ident *id = &vd->names.items[k];
                    Var *v = new_var(id->name, t, cur ? VK_LOCAL : VK_GLOBAL, id->pos);
                    if (in_block || is_alias) v->cname = sfmt("L%d_%s", v->id, id->name);
                    v->borrowed = (vd->borrowed && t->kind == T_CLASS) || is_stack;
                    v->is_stack = is_stack;
                    v->is_box = is_box;
                    v->is_alias = is_alias;
                    v->is_ref = is_alias;
                    v->decl_heap = vd->is_heap;
                    v->in_frame = has_frame;
                    if (cur) vpush(cur->locals, v);
                    else vpush(prog->globals, v);
                    add_sym(SY_VAR, id->name, id->pos)->var = v;
                    StmtVec *dst = (cur || in_block) ? local_init : &prog->init;
                    if (is_stack && !(ctor->kind == E_VAR && ctor->var->is_stack) &&
                        !(ctor->kind == E_RCALL && ctor->cmode == CM_NEWCLASS)) { /* heap object moves onto the stack */
                        Stmt *ms = new_stmt(S_ASSIGN, vd->init->pos);
                        ms->bind = 3;
                        ms->a = var_expr(v, id->pos);
                        ms->b = ctor;
                        vpush(*dst, ms);
                    } else if (is_stack && ctor->kind == E_VAR) { /* move the other stack object into this one */
                        Stmt *ms = new_stmt(S_ASSIGN, vd->init->pos);
                        ms->bind = 2;
                        ms->a = var_expr(v, id->pos);
                        ms->b = ctor;
                        vpush(*dst, ms);
                    } else if (is_stack) { /* construct the object in its storage in the frame */
                        Expr *c = NEW(Expr);
                        *c = *ctor;
                        c->cmode = CM_CTOR_INST;
                        c->a = var_expr(v, id->pos);
                        c->ctype = NULL;
                        c->type = v->type;
                        Stmt *cs = new_stmt(S_CALL, vd->init->pos);
                        cs->a = c;
                        vpush(*dst, cs);
                    } else if (is_alias) {
                        /* bind the reference to the variable it names */
                        Expr *src = r ? r : value(vd->init);
                        if (!is_lvalue(src)) error_at(vd->init->pos, "'%s' must refer to a variable, not to a computed value", id->name);
                        if (!var_compat(t, src->type))
                            error_at(vd->init->pos, "'%s' (%s) cannot refer to a variable of type \"%s\"", id->name, type_name(t),
                                     type_name(src->type));
                        if (!is_const) check_writable(src, sfmt("reference '%s'", id->name));
                        Stmt *bs = new_stmt(S_ASSIGN, vd->init->pos);
                        bs->bind = 1;
                        bs->a = var_expr(v, id->pos);
                        bs->b = src;
                        vpush(*dst, bs);
                    } else {
                        if (!r && vd->init->kind == E_TRY) try_site = vd->init;
                        add_init(dst, v, r ? r : vd->init);
                    }
                    v->is_const = is_const;
                }
                RECOVER_FAIL(for (int k = 0; k < vd->names.len; k++) vpush(broken_names, vd->names.items[k].name))
            }
            break;
        case D_ROUTINE: routine_decl(d->rt); break;
        }
    }
}

static void decls_tail(Block *b) {
    (void)b;
    if (in_iface) return; /* the implementation defines them */
    for (int i = 0; i < scope->syms.len; i++) {
        Sym *s = scope->syms.items[i];
        if (s->kind == SY_ROUTINE && s->rt->forward && !s->rt->defined)
            error_at(s->rt->pos, "forward declaration of \"%s\" not resolved", s->name);
    }
}

/* ---- stage 4: every generic is checked once on its own, with placeholder types ---- */

/* the placeholder for a type parameter: it can do exactly what its constraints allow */
static Type *archetype(Generic *g, int i) {
    int flags = 0;
    if (i < g->cons.len)
        for (int k = 0; k < g->cons.items[i].len; k++) {
            const char *c = g->cons.items[i].items[k].name;
            if (STREQ(c, "class")) return prog->tobject->type;
            else if (STREQ(c, "Copy")) flags |= PC_COPY;
            else if (STREQ(c, "Clone")) flags |= PC_CLONE;
            else if (STREQ(c, "Equatable")) flags |= PC_EQ;
            else if (STREQ(c, "Comparable")) flags |= PC_CMP | PC_EQ;
            else if (STREQ(c, "Hashable")) flags |= PC_HASH | PC_EQ;
            else if (STREQ(c, "Numeric")) flags |= PC_NUM | PC_CMP | PC_EQ;
            else { /* a class: the placeholder is that class (its members are available) */
                Look L = lookup(c);
                if (L.kind == LK_SYM && L.sym->kind == SY_TYPE) return L.sym->type;
            }
        }
    Type *t = new_type(T_PARAM, g->params.items[i].name);
    t->ik = flags;
    return t;
}

static void check_generic_alone(Generic *g) {
    int n = g->params.len;
    Type **args = (Type **)xcalloc(n ? n : 1, sizeof(Type *));
    for (int i = 0; i < n; i++) args[i] = archetype(g, i);
    int before = g_errors;
    archetype_mode++;
    g_param_owning++;
    if (g->is_routine) {
        RECOVER_BEGIN
        self_check = g;
        instantiate_routine(g, args, n, g->pos);
        self_check = NULL;
        if (g->arch_rd) {
            Scope *ss;
            Routine *sc;
            int sl;
            enter_generic(g, args, &ss, &sc, &sl);
            g_err_context = inst_name(g, args, n);
            g->arch_rt->forward = 0;
            routine_body(g->arch_rt, g->arch_rd);
            g_err_context = NULL;
            leave_generic(ss, sc, sl);
        }
        RECOVER_END
        self_check = NULL;
    } else {
        Type *volatile t = NULL;
        RECOVER_BEGIN
        t = instantiate(g, args, n, g->pos);
        RECOVER_END
        for (volatile int i = 0; t && i < g->impls.len; i++) {
            RECOVER_BEGIN
            Scope *ss;
            Routine *sc;
            int sl;
            enter_generic(g, args, &ss, &sc, &sl);
            RoutineDecl *d = reparse_routine(g->impls.items[i]->toks, g->impls.items[i]->tok_start);
            inst_cls = t->st;
            g_err_context = sfmt("%s.%s", t->st->name, d->name.name);
            routine_decl(d);
            g_err_context = NULL;
            inst_cls = NULL;
            leave_generic(ss, sc, sl);
            RECOVER_END
        }
    }
    archetype_mode--;
    g_param_owning--;
    if (g_errors > before) g->broken = 1; /* reported once here, not again for every instance */
}

/* a module: the interface goes into the shared module scope, the implementation into a private
 * scope below it (it sees the interface, importers do not see it) */
static void module_decls(Block *m) {
    Scope *pub = scope;
    int first_gen = all_generics.len;
    in_iface = 1;
    decls(m, 0);
    in_iface = 0;
    if (!m->impl) return;
    iface_scope = pub;
    Scope *priv = push_scope();
    decls(m->impl, 0);
    for (int i = first_gen; i < all_generics.len; i++)
        if (all_generics.items[i]->scope == pub) all_generics.items[i]->scope = priv; /* bodies may use private helpers */
    scope = pub;
    iface_scope = NULL;
    for (int i = 0; i < pub->syms.len; i++) {
        Sym *s = pub->syms.items[i];
        if (s->kind == SY_ROUTINE && s->rt->forward && !s->rt->defined)
            error_at(s->rt->pos, "'%s' is declared in the interface but not implemented", s->name);
        if (s->kind == SY_GENERIC && s->gen->iface_only)
            error_at(s->gen->pos, "'%s' is declared in the interface but not implemented", s->name);
    }
}

/* ============================================================== program */

static void add_type(const char *name, Type *t) { add_sym(SY_TYPE, name, (Pos){0, 0, NULL})->type = t; }

Program *sema_program(Block *prelude, Block **libs, int nlibs, Block *pb, const char *name) {
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
    t_lit = new_type(T_LIT, "literal");

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
    Sym *s = add_sym(SY_CONST, "True", (Pos){0, 0, NULL});
    s->cval = lit_int(1, t_bool, (Pos){0, 0, NULL});
    s = add_sym(SY_CONST, "False", (Pos){0, 0, NULL});
    s->cval = lit_int(0, t_bool, (Pos){0, 0, NULL});
    s = add_sym(SY_CONST, "MaxInt", (Pos){0, 0, NULL});
    s->cval = lit_int(INT_MAX, t_int, (Pos){0, 0, NULL});

    in_prelude = 1;
    decls(prelude, 0);
    in_prelude = 0;

    push_scope(); /* units ('uses ppc_std'): the program may shadow their names */
    for (int i = 0; i < nlibs; i++) module_decls(libs[i]);
    gscope = push_scope();
    decls(pb, 0);
    for (int i = 0; i < all_generics.len; i++) check_generic_alone(all_generics.items[i]);
    stmts(&pb->body);
    for (int k = 0; k < pending_bodies.len; k++) { /* bodies of generic instances (may add more) */
        RECOVER_BEGIN
        run_pending_body(k);
        RECOVER_END
    }
    pending_bodies.len = 0;
    check_settled(prog->globals.items, prog->globals.len);
    prog->body = pb->body;

    for (int i = 0; i < prog->routines.len; i++) {
        Routine *r = prog->routines.items[i];
        if (r->owner && !r->defined && !g_errors) /* after errors: possibly skipped instance bodies */
            error_at(r->pos, "method \"%s.%s\" is declared but not implemented", r->owner->name, r->name);
    }
    return prog;
}
