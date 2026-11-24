/* codegen.c - C code generation from the resolved AST */
#include "ast.h"
#include <limits.h>

extern const char *PPC_RUNTIME;

static Program *prog;
static Buf b_fwd, b_types, b_vmtt, b_globals, b_protos, b_vmti, b_frames, b_funcs, b_drops;
static Buf *out;
static Routine *cur;
static int level;
static int indent;
static int tmpc;
static VEC(int) loops; /* continue label id, or 0 for native 'continue' */
static Routine *tobject_free, *tobject_destroy;

static char *gx(Expr *e);
static void gs(Stmt *s);

static char *ctype(Type *t);

/* runtime nil check around a reference-valued expression */
/* Only needed when the compile-time nil analysis is switched off (--no-nil-check):
 * with the analysis on, every dereferenced reference is proven non-nil. */
static char *nilchk(Expr *base, char *code, Pos p) {
    if (g_nil_check) return code;
    Expr *x = base;
    while (x->kind == E_CONV || x->kind == E_CAST) x = x->a;
    if (x->kind == E_VAR && (x->var->vkind == VK_SELF || x->var->vkind == VK_WITH)) return code;
    if (x->kind == E_RCALL && x->rt->kind == RK_CTOR) return code;
    if (x->kind == E_ADDR) return code;
    return sfmt("((%s)__nilchk((void*)(%s), %d, %d))", ctype(base->type), code, p.line, p.col);
}

static void line(const char *fmt, ...) {
    for (int i = 0; i < indent; i++) buf_put(out, "  ");
    va_list ap;
    va_start(ap, fmt);
    char tmp[4096];
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof tmp) {
        char *big = xmalloc((size_t)n + 1);
        va_start(ap, fmt);
        vsnprintf(big, (size_t)n + 1, fmt, ap);
        va_end(ap);
        buf_put(out, big);
        free(big);
    } else
        buf_put(out, tmp);
    buf_put(out, "\n");
}

/* ============================================================== types */

typedef struct {
    char *key;
    char *name;
} ArrDef;
static VEC(ArrDef) arrs;

static char *ctype(Type *t);

static void ensure_struct(Struct *s) {
    if (s->emitted) return;
    s->emitted = 1;
    for (int i = 0; i < s->fields.len; i++) {
        Type *ft = s->fields.items[i]->type;
        if (ft->kind == T_RECORD) ensure_struct(ft->st);
        else if (ft->kind == T_ARRAY) ctype(ft);
    }
    Buf b = {0};
    buf_printf(&b, "struct S%d { /* %s */\n", s->id, s->name);
    for (int i = 0; i < s->fields.len; i++) {
        Field *f = s->fields.items[i];
        if (STREQ(f->cname, "__vmt")) buf_put(&b, "  void *__vmt;\n");
        else buf_printf(&b, "  %s %s;\n", ctype(f->type), f->cname);
    }
    if (!s->fields.len) buf_put(&b, "  char __dummy;\n");
    buf_put(&b, "};\n");
    buf_put(&b_types, buf_str(&b));
}

static char *ctype(Type *t) {
    switch (t->kind) {
    case T_VOID: return "void";
    case T_INT:
        switch (t->ik) {
        case IK_I8: return "int8_t";
        case IK_U8: return "uint8_t";
        case IK_I16: return "int16_t";
        case IK_U16: return "uint16_t";
        case IK_I32: return "int32_t";
        case IK_U32: return "uint32_t";
        default: return "int64_t";
        }
    case T_BOOL: return "uint8_t";
    case T_CHAR: return "uint8_t";
    case T_REAL: return "double";
    case T_STR: return "PStr";
    case T_ENUM: return "int32_t";
    case T_NIL: return "void*";
    case T_OPT: { /* value optional: struct { has; value } (like std::optional) */
        char *elem = ctype(t->base);
        char *key = sfmt("opt|%s", elem);
        for (int i = 0; i < arrs.len; i++)
            if (STREQ(arrs.items[i].key, key)) return sfmt("struct %s", arrs.items[i].name);
        if (t->base->kind == T_RECORD) ensure_struct(t->base->st);
        ArrDef d = {key, sfmt("O%d", arrs.len + 1)};
        vpush(arrs, d);
        buf_printf(&b_fwd, "struct %s;\n", d.name);
        buf_printf(&b_types, "struct %s { uint8_t has; %s v; };\n", d.name, elem);
        return sfmt("struct %s", d.name);
    }
    case T_PTR: return t->base ? sfmt("%s*", ctype(t->base)) : "void*";
    case T_RECORD: return sfmt("struct S%d", t->st->id);
    case T_CLASS: return sfmt("struct S%d*", t->st->id);
    case T_ARRAY: {
        char *elem = ctype(t->base);
        long long n = t->hi - t->lo + 1;
        char *key = sfmt("%s|%lld", elem, n);
        for (int i = 0; i < arrs.len; i++)
            if (STREQ(arrs.items[i].key, key)) return sfmt("struct %s", arrs.items[i].name);
        if (t->base->kind == T_RECORD) ensure_struct(t->base->st);
        ArrDef d = {key, sfmt("A%d", arrs.len + 1)};
        vpush(arrs, d);
        buf_printf(&b_fwd, "struct %s;\n", d.name);
        buf_printf(&b_types, "struct %s { %s a[%lld]; };\n", d.name, elem, n);
        return sfmt("struct %s", d.name);
    }
    }
    return "void";
}

/* ============================================================== helpers */

static char *c_strlit(const char *s, int n) {
    Buf b = {0};
    buf_put(&b, "\"");
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') buf_printf(&b, "\\%c", c);
        else if (c == '?') buf_put(&b, "\\?");
        else if (c >= 32 && c < 127) buf_putn(&b, (char *)&c, 1);
        else buf_printf(&b, "\\%03o", c);
    }
    buf_put(&b, "\"");
    return b.d;
}

static char *frame_ptr(int m) {
    if (m == level) return "(&__f)";
    char *s = "__up";
    for (int i = 0; i < level - 1 - m; i++) s = sfmt("%s->__up", s);
    return s;
}
static char *var_raw(Var *v) {
    if (v->is_global) return v->cname;
    if (v->level == level) return v->in_frame ? sfmt("__f.%s", v->cname) : v->cname;
    return sfmt("%s->%s", frame_ptr(v->level), v->cname);
}
static char *var_ref(Var *v) {
    char *r = var_raw(v);
    return v->is_ref ? sfmt("(*%s)", r) : r;
}
/* auxiliary storage of a variable (stack object storage / live flag) */
static char *var_aux(Var *v, const char *suffix) {
    Var t = *v;
    t.cname = sfmt("%s%s", v->cname, suffix);
    t.is_ref = 0;
    return var_raw(&t);
}

/* ---- automatic freeing (like dropping a Box in Rust) ---- */

/* code that frees an owned reference */
static char *drop_code(Type *t, const char *x) {
    if (t->kind == T_CLASS) return sfmt("%s((struct S%d*)(%s))", tobject_free->cname, tobject_free->owner->id, x);
    return sfmt("__ppc_free(%s)", x);
}

/* variable that owns what it refers to (and frees it at the end of its scope) */
static int owner_var(Var *v) {
    if (!is_ref_type(v->type) || v->borrowed || v->is_stack) return 0;
    if (v->vkind == VK_LOCAL || v->vkind == VK_GLOBAL || v->vkind == VK_RESULT) return 1;
    return v->vkind == VK_PARAM && (v->mode == PM_OWNED || v->mode == PM_VAR || v->mode == PM_OUT);
}

/* assignment target that owns what is stored in it */
static int owning_place(Expr *e) {
    if (!is_ref_type(e->type)) return 0;
    if (e->kind == E_VAR) return owner_var(e->var);
    if (e->kind == E_FIELD) return !e->field->borrowed;
    return e->kind == E_INDEX || e->kind == E_DEREF;
}

/* owner variable whose value moves away when it is used as a source */
static Var *move_source(Expr *e) {
    while (e->kind == E_CONV || e->kind == E_CAST || e->kind == E_AS) e = e->a;
    if (e->kind == E_VAR && owner_var(e->var) && e->var->vkind != VK_RESULT) return e->var;
    return NULL;
}

static int droppable_lvalue(Expr *e) {
    if (e->kind == E_VAR) return e->var->vkind != VK_SELF && e->var->vkind != VK_WITH && !e->var->is_stack;
    return e->kind == E_FIELD || e->kind == E_INDEX || e->kind == E_DEREF;
}


static char *int_lit(long long v) {
    if (v == LLONG_MIN) return "(-9223372036854775807LL-1)";
    if (v < 0) return v >= INT_MIN ? sfmt("(%lld)", v) : sfmt("(%lldLL)", v);
    return v <= INT_MAX ? sfmt("%lld", v) : sfmt("%lldLL", v);
}

static char *real_lit(double d) {
    char b[64];
    snprintf(b, sizeof b, "%.17g", d);
    if (!strpbrk(b, ".eEni")) strcat(b, ".0");
    if (d < 0) return sfmt("(%s)", b);
    return xstrdup(b);
}

static Routine *find_method(Struct *s, const char *name) {
    for (; s; s = s->parent)
        for (int i = 0; i < s->methods.len; i++)
            if (STREQ(s->methods.items[i]->name, name)) return s->methods.items[i];
    return NULL;
}

/* ============================================================== calls */

static char *gcall_core(Expr *e, const char *selfp, Struct *sst, const char *as);


static int cg_lvalue(Expr *e) {
    switch (e->kind) {
    case E_VAR: return 1;
    case E_FIELD: return e->a->type->kind == T_CLASS || cg_lvalue(e->a);
    case E_INDEX: case E_CAST: return cg_lvalue(e->a);
    case E_DEREF: return 1;
    default: return 0;
    }
}

static char *ret_ctype(Routine *r);

static char *gcall(Expr *e) {
    Routine *r = e->rt;
    char *selfp = NULL;
    Struct *sst = NULL;
    Buf pre = {0}, post = {0};
    if (e->a) {
        Type *bt = e->a->type;
        char *bv;
        if (bt->kind == T_CLASS && is_owning_temp(e->a)) { /* temporary receiver: freed after the call */
            int id = ++tmpc;
            buf_printf(&pre, "%s __rv%d = %s; ", ctype(bt), id, gx(e->a));
            bv = sfmt("__rv%d", id);
            buf_printf(&post, "%s; ", drop_code(bt, bv));
        } else {
            bv = gx(e->a);
            if (bt->kind == T_CLASS && r != tobject_free && !(e->a->kind == E_VAR && e->a->var->is_stack))
                bv = nilchk(e->a, bv, e->pos);
        }
        selfp = bt->kind == T_CLASS ? bv : sfmt("(&%s)", bv);
        sst = bt->st;
        /* explicit Free/Destroy: the place owns nothing afterwards */
        int is_free = bt->kind == T_CLASS && (r == tobject_free || (r->kind == RK_DTOR && e->cmode != CM_INHERITED));
        if (is_free && droppable_lvalue(e->a)) buf_printf(&post, "%s = 0; ", gx(e->a));
    }
    Buf args = {0};
    for (int i = 0; i < e->args.len; i++) {
        Param *pm = &r->params.items[i];
        Expr *a = e->args.items[i];
        buf_put(&args, ", ");
        if (pm->mode == PM_VAR || pm->mode == PM_OUT) {
            buf_printf(&args, "((%s*)&%s)", ctype(pm->type), gx(a));
        } else if (is_ref_type(pm->type) && pm->mode != PM_OWNED && is_owning_temp(a)) {
            /* owned temporary passed to a borrowing parameter: freed after the call */
            int id = ++tmpc;
            buf_printf(&pre, "%s __ta%d = %s; ", ctype(pm->type), id, gx(a));
            buf_printf(&args, pm->mode == PM_CONSTREF ? "&__ta%d" : "__ta%d", id);
            buf_printf(&post, "%s; ", drop_code(pm->type, sfmt("__ta%d", id)));
        } else if (pm->mode == PM_CONSTREF) {
            /* read-only by reference: the variable itself, or a temporary copy of a computed value */
            if (cg_lvalue(a)) buf_printf(&args, "((%s*)&%s)", ctype(pm->type), gx(a));
            else buf_printf(&args, "(&((%s[1]){%s})[0])", ctype(pm->type), gx(a));
        } else {
            buf_put(&args, gx(a));
            Var *mv = pm->mode == PM_OWNED ? move_source(a) : NULL;
            if (mv) buf_printf(&post, "%s = 0; ", var_ref(mv)); /* ownership moved into the callee */
        }
    }
    if (pre.len || post.len) {
        char *call = gcall_core(e, selfp, sst, buf_str(&args));
        int id = ++tmpc;
        int is_void = e->cmode != CM_NEWCLASS && STREQ(ret_ctype(r), "void");
        if (is_void) return sfmt("({ %s%s; %s})", buf_str(&pre), call, buf_str(&post));
        return sfmt("({ %s__typeof__(%s) __cr%d = %s; %s__cr%d; })", buf_str(&pre), call, id, call, buf_str(&post), id);
    }
    if (e->a && e->a->type->kind == T_RECORD && !cg_lvalue(e->a)) {
        /* method call on a temporary record value: give it an address first */
        int id = ++tmpc;
        char *tv = sfmt("__rt%d", id);
        char *call = gcall_core(e, sfmt("(&%s)", tv), sst, buf_str(&args));
        return sfmt("({ %s %s = %s; %s; })", ctype(e->a->type), tv, gx(e->a), call);
    }
    return gcall_core(e, selfp, sst, buf_str(&args));
}

static char *gcall_core(Expr *e, const char *selfp, Struct *sst, const char *as) {
    Routine *r = e->rt;
    const char *dflag = "";
    switch (e->cmode) {
    case CM_NORMAL:
        if (r->level > 1) return sfmt("%s(%s%s)", r->cname, frame_ptr(r->level - 1), as);
        return sfmt("%s(%s)", r->cname, as[0] ? as + 2 : "");
    case CM_NEWCLASS:
        return sfmt("((%s)%s(NULL, &VMTI%d%s))", ctype(e->ctype), r->cname, e->ctype->st->id, as);
    case CM_CTOR_INST: {
        char *call = sfmt("%s((struct S%d*)%s, NULL%s)", r->cname, r->owner->id, selfp, as);
        if (e->a->kind == E_VAR && e->a->var->is_stack) /* stack object: construct in place */
            return sfmt("(%s, %s = 1, %s)", call, var_aux(e->a->var, "__live"), selfp);
        return call;
    }
    case CM_VIRTUAL:
        if (r->kind == RK_DTOR) dflag = ", 1";
        return sfmt("((struct VMT%d*)((%s)->__vmt))->v%d(%s%s%s)", sst->id, selfp, r->slot, selfp, dflag, as);
    case CM_STATIC:
        if (r->kind == RK_DTOR) dflag = ", 1";
        return sfmt("%s((struct S%d*)%s%s%s)", r->cname, r->owner->id, selfp, dflag, as);
    case CM_INHERITED:
        if (r->kind == RK_CTOR) dflag = ", NULL";
        if (r->kind == RK_DTOR) dflag = ", 0";
        return sfmt("%s((struct S%d*)%s%s%s)", r->cname, r->owner->id, selfp, dflag, as);
    }
    return "0";
}

/* ============================================================== builtins */

static char *write_arg(Expr *a) {
    Expr *v = a;
    const char *w = "0", *p = "-1";
    if (a->kind == E_FMT) {
        v = a->a;
        w = gx(a->b);
        if (a->c) p = gx(a->c);
    }
    char *x = gx(v);
    switch (v->type->kind) {
    case T_INT: return sfmt("__w_int((int64_t)(%s), %s)", x, w);
    case T_REAL: return sfmt("__w_real(%s, %s, %s)", x, w, p);
    case T_BOOL: return sfmt("__w_bool(%s, %s)", x, w);
    case T_CHAR: return sfmt("__w_char(%s, %s)", x, w);
    case T_STR: return sfmt("__w_str(%s, %s)", x, w);
    case T_ENUM: return sfmt("__w_cstr(ENAMES%d[%s], %s)", v->type->en->id, x, w);
    default: return "((void)0)";
    }
}

static char *gbuiltin(Expr *e) {
    ExprVec *A = &e->args;
#define G(i) gx(A->items[i])
    switch (e->bi) {
    case BI_WRITE: case BI_WRITELN: {
        Buf b = {0};
        buf_put(&b, "(");
        for (int i = 0; i < A->len; i++) {
            if (i) buf_put(&b, ", ");
            buf_put(&b, write_arg(A->items[i]));
        }
        if (e->bi == BI_WRITELN) buf_put(&b, A->len ? ", __w_ln()" : "__w_ln()");
        else if (!A->len) buf_put(&b, "(void)0");
        buf_put(&b, ")");
        return b.d;
    }
    case BI_READ: case BI_READLN: {
        Buf b = {0};
        buf_put(&b, "(");
        for (int i = 0; i < A->len; i++) {
            Expr *a = A->items[i];
            if (i) buf_put(&b, ", ");
            const char *f = a->type->kind == T_REAL ? "__r_real()"
                            : a->type->kind == T_CHAR ? "__r_char()"
                            : a->type->kind == T_STR ? "__r_str()" : "__r_int()";
            buf_printf(&b, "(%s = (%s)%s)", gx(a), ctype(a->type), f);
        }
        if (e->bi == BI_READLN) buf_put(&b, A->len ? ", __r_ln()" : "__r_ln()");
        else if (!A->len) buf_put(&b, "(void)0");
        buf_put(&b, ")");
        return b.d;
    }
    case BI_LENGTH:
        if (A->items[0]->type->kind == T_CHAR) return "1";
        return sfmt("((int32_t)(%s).s[0])", G(0));
    case BI_ORD: return sfmt("((%s)(%s))", ctype(e->type), G(0));
    case BI_CHR: return sfmt("((uint8_t)(%s))", G(0));
    case BI_SUCC: return sfmt("((%s)((%s) + 1))", ctype(e->type), G(0));
    case BI_PRED: return sfmt("((%s)((%s) - 1))", ctype(e->type), G(0));
    case BI_ODD: return sfmt("((((int64_t)(%s)) & 1) != 0)", G(0));
    case BI_INC: case BI_DEC:
        return sfmt("(%s %s %s)", G(0), e->bi == BI_INC ? "+=" : "-=", A->len > 1 ? G(1) : "1");
    case BI_ABS: return A->items[0]->type->kind == T_REAL ? sfmt("fabs(%s)", G(0)) : sfmt("__absi(%s)", G(0));
    case BI_SQR: return A->items[0]->type->kind == T_REAL ? sfmt("__sqrr(%s)", G(0)) : sfmt("__sqri(%s)", G(0));
    case BI_SQRT: return sfmt("sqrt(%s)", G(0));
    case BI_SIN: return sfmt("sin(%s)", G(0));
    case BI_COS: return sfmt("cos(%s)", G(0));
    case BI_ARCTAN: return sfmt("atan(%s)", G(0));
    case BI_EXP: return sfmt("exp(%s)", G(0));
    case BI_LN: return sfmt("log(%s)", G(0));
    case BI_TRUNC: return sfmt("((int64_t)(%s))", G(0));
    case BI_ROUND: return sfmt("__round(%s)", G(0));
    case BI_FRAC: return sfmt("__frac(%s)", G(0));
    case BI_INT: return sfmt("trunc(%s)", G(0));
    case BI_SIZEOF: return sfmt("((int32_t)sizeof(%s))", ctype(e->ctype));
    case BI_COPY: return sfmt("__pstr_copy(%s, %s, %s)", G(0), G(1), G(2));
    case BI_POS: return sfmt("__pstr_pos(%s, %s)", G(0), G(1));
    case BI_DELETE: return sfmt("__pstr_delete(&%s, %s, %s)", G(0), G(1), G(2));
    case BI_INSERT: return sfmt("__pstr_insert(%s, &%s, %s)", G(0), G(1), G(2));
    case BI_INTTOSTR: return sfmt("__int_to_str(%s)", G(0));
    case BI_STRTOINT: return sfmt("__str_to_int(%s)", G(0));
    case BI_FLOATTOSTR: return sfmt("__float_to_str(%s)", G(0));
    case BI_STR: {
        Expr *a = A->items[0];
        Expr *v = a->kind == E_FMT ? a->a : a;
        const char *w = a->kind == E_FMT ? gx(a->b) : "0";
        const char *p = a->kind == E_FMT && a->c ? gx(a->c) : "-1";
        if (v->type->kind == T_REAL) return sfmt("(%s = __str_real(%s, %s, %s))", G(1), gx(v), w, p);
        return sfmt("(%s = __str_int(%s, %s))", G(1), gx(v), w);
    }
    case BI_UPCASE:
        return A->items[0]->type->kind == T_CHAR ? sfmt("__upcase(%s)", G(0)) : sfmt("__upper(%s)", G(0));
    case BI_UPPERCASE: return sfmt("__upper(%s)", G(0));
    case BI_LOWERCASE: return sfmt("__lower(%s)", G(0));
    case BI_CONCAT: {
        char *s = G(0);
        for (int i = 1; i < A->len; i++) s = sfmt("__pstr_cat(%s, %s)", s, G(i));
        return s;
    }
    case BI_HALT: return sfmt("(fflush(stdout), exit((int)(%s)))", A->len ? G(0) : "0");
    case BI_RANDOM: return A->len ? sfmt("__random_int(%s)", G(0)) : "__random_real()";
    case BI_RANDOMIZE: return "__randomize()";
    case BI_ASSIGNED: case BI_HASVALUE:
        if (A->items[0]->type->kind == T_OPT) return sfmt("((%s).has != 0)", G(0));
        return sfmt("((%s) != 0)", G(0));
    case BI_BOXNEW: { /* heap value: allocate a zeroed cell, optionally initialize it */
        int id = ++tmpc;
        char *ct = ctype(e->ctype);
        if (A->len) return sfmt("({ %s *__b%d = (%s*)__ppc_alloc(sizeof(%s)); *__b%d = %s; __b%d; })", ct, id, ct, ct, id, G(0), id);
        return sfmt("((%s*)__ppc_alloc(sizeof(%s)))", ct, ct);
    }
    case BI_BOXFREE: return sfmt("(__ppc_free(%s), %s = 0)", G(0), G(0));
    case BI_VALUEOR: {
        int id = ++tmpc;
        return sfmt("({ %s __o%d = %s; __o%d.has ? __o%d.v : (%s); })", ctype(A->items[0]->type), id, G(0), id, id, G(1));
    }
    case BI_NEW: {
        Expr *a = A->items[0];
        Type *pt = a->kind == E_TYPEREF ? a->ctype : a->type;
        Type *bt = pt->base;
        int id = ++tmpc;
        char *tn = sfmt("__n%d", id);
        Buf b = {0};
        buf_printf(&b, "({ %s %s = (%s)__ppc_alloc(sizeof(%s)); ", ctype(pt), tn, ctype(pt), ctype(bt));
        if (a->kind != E_TYPEREF) buf_printf(&b, "%s = %s; ", gx(a), tn);
        buf_printf(&b, "%s; })", a->kind == E_TYPEREF ? tn : "(void)0");
        return b.d;
    }
    case BI_DISPOSE:
        return sfmt("__ppc_free(%s)", G(0));
    case BI_FREEANDNIL: {
        int id = ++tmpc;
        char *l = G(0);
        return sfmt("({ void *__t%d = (void*)%s; %s = 0; %s((struct S%d*)__t%d); })", id, l, l, tobject_free->cname,
                    tobject_free->owner->id, id);
    }
    case BI_TAKE: {
        int id = ++tmpc;
        char *l = G(0);
        return sfmt("({ %s __t%d = %s; %s = 0; __t%d; })", ctype(e->type), id, l, l, id);
    }
    case BI_SETLENGTH: return sfmt("((%s).s[0] = (uint8_t)(%s))", G(0), G(1));
    case BI_CLASSNAME: return sfmt("__pstr_cstr((*(struct __VMTHdr**)(%s))->name)", G(0));
    }
#undef G
    return "0";
}

/* ============================================================== expressions */

static const char *binop_str(int op) {
    switch (op) {
    case OP_ADD: return "+";
    case OP_SUB: return "-";
    case OP_MUL: return "*";
    case OP_DIV: return "/";
    case OP_IDIV: return "/";
    case OP_MOD: return "%";
    case OP_EQ: return "==";
    case OP_NE: return "!=";
    case OP_LT: return "<";
    case OP_LE: return "<=";
    case OP_GT: return ">";
    case OP_GE: return ">=";
    }
    return "?";
}

static int is_cmp(int op) { return op >= OP_EQ && op <= OP_GE; }

static char *gx(Expr *e) {
    switch (e->kind) {
    case E_INT:
        if (e->type->kind == T_BOOL) return e->ival ? "1" : "0";
        return int_lit(e->ival);
    case E_REAL: return real_lit(e->rval);
    case E_STR:
        if (e->type->kind == T_CHAR) return sfmt("%d", (unsigned char)e->sval[0]);
        return sfmt("__pstr_lit(%s, %d)", c_strlit(e->sval, e->slen), e->slen);
    case E_NIL: return "((void*)0)";
    case E_VAR: return var_ref(e->var);
    case E_FIELD:
        if (e->a->type->kind == T_CLASS && is_owning_temp(e->a)) { /* field of a temporary object: read, then free it */
            int id = ++tmpc;
            return sfmt("({ %s __ft%d = %s; %s __fv%d = __ft%d->%s; %s; __fv%d; })", ctype(e->a->type), id, gx(e->a),
                        ctype(e->type), id, id, e->field->cname, drop_code(e->a->type, sfmt("__ft%d", id)), id);
        }
        if (e->a->type->kind == T_CLASS) return sfmt("(%s)->%s", nilchk(e->a, gx(e->a), e->pos), e->field->cname);
        return sfmt("(%s).%s", gx(e->a), e->field->cname);
    case E_INDEX: {
        Expr *a = e->a;
        char *idx = gx(e->args.items[0]);
        if (a->type->kind == T_STR) return sfmt("(%s).s[%s]", gx(a), idx);
        if (a->type->lo == 0) return sfmt("(%s).a[%s]", gx(a), idx);
        return sfmt("(%s).a[(%s) - (%s)]", gx(a), idx, int_lit(a->type->lo));
    }
    case E_DEREF: return sfmt("(*(%s))", nilchk(e->a, gx(e->a), e->pos));
    case E_ADDR: return sfmt("(&%s)", gx(e->a));
    case E_UNARY:
        if (e->op == OP_NEG) return sfmt("(-(%s))", gx(e->a));
        if (e->op == OP_PLUS) return gx(e->a);
        if (e->type->kind == T_BOOL) return sfmt("(!(%s))", gx(e->a));
        return sfmt("(~(%s))", gx(e->a));
    case E_BINARY: {
        Type *ta = e->a->type;
        char *a = gx(e->a), *b = gx(e->b);
        int op = e->op;
        if (ta->kind == T_STR) {
            if (op == OP_ADD) return sfmt("__pstr_cat(%s, %s)", a, b);
            return sfmt("(__pstr_cmp(%s, %s) %s 0)", a, b, binop_str(op));
        }
        if (is_cmp(op) && (ta->kind == T_OPT || e->b->type->kind == T_OPT)) { /* opt = none / opt <> none */
            Expr *o = ta->kind == T_OPT ? e->a : e->b;
            return sfmt("((%s).has %s 0)", gx(o), op == OP_EQ ? "==" : "!=");
        }
        if (is_cmp(op) && (ta->kind == T_PTR || ta->kind == T_CLASS || ta->kind == T_NIL))
            return sfmt("((void*)(%s) %s (void*)(%s))", a, binop_str(op), b);
        if (op == OP_AND || op == OP_OR || op == OP_XOR) {
            if (e->type->kind == T_BOOL) {
                if (op == OP_XOR) return sfmt("(!(%s) != !(%s))", a, b);
                return sfmt("(%s %s %s)", a, op == OP_AND ? "&&" : "||", b);
            }
            return sfmt("((%s)(%s) %s (%s))", ctype(e->type), a, op == OP_AND ? "&" : op == OP_OR ? "|" : "^", b);
        }
        if (op == OP_SHL) return sfmt("((%s)((%s)(%s) << (%s)))", ctype(e->type), ctype(e->type), a, b);
        if (op == OP_SHR) {
            if (e->type->ik == IK_I64) return sfmt("((int64_t)((uint64_t)(%s) >> (%s)))", a, b);
            return sfmt("((int32_t)((uint32_t)(%s) >> (%s)))", a, b);
        }
        if (!is_cmp(op) && e->type->kind == T_INT && e->type->ik == IK_I64)
            return sfmt("((int64_t)(%s) %s (%s))", a, binop_str(op), b);
        return sfmt("(%s %s %s)", a, binop_str(op), b);
    }
    case E_IN: {
        int id = ++tmpc;
        Expr *s = e->b;
        Buf c = {0};
        for (int i = 0; i < s->args.len; i++) {
            if (i) buf_put(&c, " || ");
            if (s->args2.items[i])
                buf_printf(&c, "(__i%d >= %s && __i%d <= %s)", id, gx(s->args.items[i]), id, gx(s->args2.items[i]));
            else
                buf_printf(&c, "__i%d == %s", id, gx(s->args.items[i]));
        }
        if (!s->args.len) buf_put(&c, "0");
        return sfmt("({ int64_t __i%d = (%s); (%s); })", id, gx(e->a), buf_str(&c));
    }
    case E_IS: return sfmt("__is((void*)(%s), &VMTI%d)", gx(e->a), e->ctype->st->id);
    case E_AS: return sfmt("((struct S%d*)__as((void*)(%s), &VMTI%d))", e->ctype->st->id, gx(e->a), e->ctype->st->id);
    case E_ISSOME: {
        Var *k = e->var;
        if (e->a->type->kind == T_OPT) {
            int id = ++tmpc;
            return sfmt("({ %s __s%d = %s; __s%d.has ? (%s = __s%d.v, 1) : 0; })", ctype(e->a->type), id, gx(e->a), id,
                        var_ref(k), id);
        }
        if (owner_var(k)) /* owning binding (from Take/a fresh object): free what it held before */
            return sfmt("({ if (%s) %s; (%s = %s) != 0; })", var_ref(k), drop_code(k->type, var_ref(k)), var_ref(k), gx(e->a));
        return sfmt("((%s = %s) != 0)", var_ref(k), gx(e->a));
    }
    case E_CONV: {
        Type *to = e->type, *from = e->a->type;
        if (to->kind == T_OPT) {
            if (from->kind == T_NIL) return sfmt("((%s){0})", ctype(to));
            return sfmt("((%s){1, %s})", ctype(to), gx(e->a));
        }
        char *a = gx(e->a);
        if (to->kind == T_REAL) return sfmt("((double)(%s))", a);
        if (to->kind == T_STR && from->kind == T_CHAR) return sfmt("__pstr_char(%s)", a);
        if (to->kind == T_PTR || to->kind == T_CLASS) return sfmt("((%s)(%s))", ctype(to), a);
        return a;
    }
    case E_CAST: {
        Type *t = e->type;
        if (t->kind == T_RECORD) return sfmt("(*(%s*)&%s)", ctype(t), gx(e->a));
        return sfmt("((%s)(%s))", ctype(t), gx(e->a));
    }
    case E_RCALL: return gcall(e);
    case E_BUILTIN: return gbuiltin(e);
    default: error_at(e->pos, "internal error: unresolved expression (kind %d)", e->kind);
    }
}

/* ============================================================== statements */

/* stack object: point the reference at its storage in the frame and set the VMT */
static void stack_init(Var *v) {
    if (!v->is_stack) return;
    line("%s = &%s;", var_raw(v), var_aux(v, "__st"));
    line("%s->__vmt = &VMTI%d;", var_raw(v), v->type->st->id);
}

/* stack objects are destroyed (Destroy without freeing) when their block ends,
 * in reverse declaration order */
static void stack_destroy(Var **vars, int n) {
    for (int i = n - 1; i >= 0; i--) {
        Var *v = vars[i];
        if (v->is_stack) {
            char *x = var_raw(v);
            line("if (%s) { ((struct VMT%d*)(%s->__vmt))->v%d(%s, 0); ((struct __VMTHdr*)%s->__vmt)->drop(%s); %s = 0; }",
                 var_aux(v, "__live"), v->type->st->id, x, tobject_destroy->slot, x, x, x, var_aux(v, "__live"));
        } else if (owner_var(v) && v->vkind != VK_RESULT && !v->is_ref) {
            char *x = var_ref(v);
            line("if (%s) { %s; %s = 0; }", x, drop_code(v->type, x), x);
        }
    }
}

/* assignment; owning places free their old value, a moved-from owner variable becomes empty */
static void gassign(Expr *lhs, Expr *rhs) {
    char *l = gx(lhs), *r = gx(rhs);
    if (!owning_place(lhs)) {
        line("%s = %s;", l, r);
        return;
    }
    Var *mv = move_source(rhs);
    if (mv && lhs->kind == E_VAR && lhs->var == mv) return; /* x := x */
    int id = ++tmpc;
    char *ct = ctype(lhs->type);
    Buf b = {0};
    buf_printf(&b, "{ %s __n%d = %s; %s __o%d = %s; %s = __n%d; ", ct, id, r, ct, id, l, l, id);
    if (mv) buf_printf(&b, "%s = 0; ", var_ref(mv));
    buf_printf(&b, "if (__o%d && (void*)__o%d != (void*)__n%d) %s; }", id, id, id, drop_code(lhs->type, sfmt("__o%d", id)));
    line("%s", buf_str(&b));
}

static void gstmts(StmtVec *v) {
    for (int i = 0; i < v->len; i++) gs(v->items[i]);
}

static void gs(Stmt *s) {
    switch (s->kind) {
    case S_EMPTY: return;
    case S_ASSIGN:
        if (s->a->kind == E_DEREF && s->a->a->kind == E_VAR && s->a->a->var->is_box) {
            /* storing into a heap value allocates its cell on first use */
            Var *v = s->a->a->var;
            char *x = var_ref(v), *ct = ctype(v->type->base);
            line("if (!%s) %s = (%s*)__ppc_alloc(sizeof(%s));", x, x, ct, ct);
            line("*%s = %s;", x, gx(s->b));
            return;
        }
        gassign(s->a, s->b);
        return;
    case S_BLOCK:
        line("{");
        indent++;
        for (int i = 0; i < s->wvars.len; i++) { /* fresh variables each time the block is entered */
            Var *v = s->wvars.items[i];
            char *x = var_raw(v);
            line("memset(&(%s), 0, sizeof(%s));", x, x);
            if (v->is_stack) {
                line("%s = 0;", var_aux(v, "__live"));
                stack_init(v);
            }
        }
        gstmts(&s->blk->body);
        stack_destroy(s->wvars.items, s->wvars.len);
        indent--;
        line("}");
        return;
    case S_CALL:
        if (is_owning_temp(s->a)) { /* an owned result nobody takes is freed right away */
            int id = ++tmpc;
            line("{ %s __d%d = %s; if (__d%d) %s; }", ctype(s->a->type), id, gx(s->a), id,
                 drop_code(s->a->type, sfmt("__d%d", id)));
        } else
            line("%s;", gx(s->a));
        return;
    case S_COMPOUND:
        line("{");
        indent++;
        gstmts(&s->body);
        indent--;
        line("}");
        return;
    case S_IF:
        line("if (%s) {", gx(s->a));
        indent++;
        gs(s->s1);
        indent--;
        if (s->s2) {
            line("} else {");
            indent++;
            gs(s->s2);
            indent--;
        }
        line("}");
        return;
    case S_WHILE:
        vpush(loops, 0);
        line("while (%s) {", gx(s->a));
        indent++;
        gs(s->s1);
        indent--;
        line("}");
        loops.len--;
        return;
    case S_REPEAT:
        vpush(loops, 0);
        line("do {");
        indent++;
        gstmts(&s->body);
        indent--;
        line("} while (!(%s));", gx(s->a));
        loops.len--;
        return;
    case S_FOR: {
        int id = ++tmpc;
        char *v = gx(s->c);
        line("{");
        indent++;
        line("int64_t __lo%d = (%s), __hi%d = (%s);", id, gx(s->a), id, gx(s->b));
        line("if (__lo%d %s __hi%d) {", id, s->down ? ">=" : "<=", id);
        indent++;
        line("%s = (%s)__lo%d;", v, ctype(s->c->type), id);
        line("for (;;) {");
        indent++;
        vpush(loops, id);
        gs(s->s1);
        loops.len--;
        line("__cont%d:;", id);
        line("if ((int64_t)%s == __hi%d) break;", v, id);
        line("%s%s;", v, s->down ? "--" : "++");
        indent--;
        line("}");
        indent--;
        line("}");
        indent--;
        line("}");
        return;
    }
    case S_CASE: {
        int id = ++tmpc;
        Type *t = s->a->type;
        int is_str = t->kind == T_STR;
        line("{");
        indent++;
        line("%s __c%d = %s;", is_str ? "PStr" : "int64_t", id, gx(s->a));
        for (int i = 0; i < s->arms.len; i++) {
            CaseArm *arm = &s->arms.items[i];
            Buf c = {0};
            for (int k = 0; k < arm->lo.len; k++) {
                if (k) buf_put(&c, " || ");
                char *lo = gx(arm->lo.items[k]);
                if (arm->hi.items[k]) {
                    char *hi = gx(arm->hi.items[k]);
                    if (is_str)
                        buf_printf(&c, "(__pstr_cmp(__c%d, %s) >= 0 && __pstr_cmp(__c%d, %s) <= 0)", id, lo, id, hi);
                    else
                        buf_printf(&c, "(__c%d >= %s && __c%d <= %s)", id, lo, id, hi);
                } else if (is_str)
                    buf_printf(&c, "__pstr_cmp(__c%d, %s) == 0", id, lo);
                else
                    buf_printf(&c, "__c%d == %s", id, lo);
            }
            line("%sif (%s) {", i ? "} else " : "", buf_str(&c));
            indent++;
            gs(arm->body);
            indent--;
        }
        if (s->has_else) {
            if (s->arms.len) line("} else {");
            else line("{");
            indent++;
            gstmts(&s->else_);
            indent--;
            line("}");
        } else if (s->arms.len)
            line("}");
        indent--;
        line("}");
        return;
    }
    case S_WITH:
        line("{");
        indent++;
        for (int i = 0; i < s->wvars.len; i++) {
            Var *w = s->wvars.items[i];
            if (w->is_ref) line("%s *%s = &%s;", ctype(w->type), w->cname, gx(s->exprs.items[i]));
            else line("%s %s = %s;", ctype(w->type), w->cname, nilchk(s->exprs.items[i], gx(s->exprs.items[i]), s->exprs.items[i]->pos));
        }
        gs(s->s1);
        indent--;
        line("}");
        return;
    case S_BREAK: line("break;"); return;
    case S_CONTINUE:
        if (loops.len && loops.items[loops.len - 1]) line("goto __cont%d;", loops.items[loops.len - 1]);
        else line("continue;");
        return;
    case S_EXIT:
        if (s->a) {
            Expr *rv = new_expr(E_VAR, s->pos);
            rv->var = cur->result;
            rv->type = cur->result->type;
            gassign(rv, s->a);
        }
        line("goto __exit;");
        return;
    }
}

/* ============================================================== routines */

static char *ret_ctype(Routine *r) {
    if (r->kind == RK_FUNC) return ctype(r->ret);
    if (r->kind == RK_CTOR && r->owner->kind == SK_CLASS) return sfmt("struct S%d*", r->owner->id);
    return "void";
}

static char *sig(Routine *r, int names) {
    Buf b = {0};
    buf_printf(&b, "%s %s(", ret_ctype(r), r->cname);
    int n = 0;
#define SEP() (n++ ? ", " : "")
    if (r->level > 1) buf_printf(&b, "%sstruct F%d *%s", SEP(), r->parent->id, names ? "__up" : "");
    if (r->owner) buf_printf(&b, "%sstruct S%d *%s", SEP(), r->owner->id, names ? "self" : "");
    if (r->kind == RK_CTOR) buf_printf(&b, "%svoid *%s", SEP(), names ? "__vmt" : "");
    if (r->kind == RK_DTOR) buf_printf(&b, "%sint %s", SEP(), names ? "__free" : "");
    for (int i = 0; i < r->params.len; i++) {
        Param *p = &r->params.items[i];
        int ref = p->mode == PM_VAR || p->mode == PM_OUT || p->mode == PM_CONSTREF;
        buf_printf(&b, "%s%s%s %s", SEP(), ctype(p->type), ref ? "*" : "", names && p->var ? p->var->cname : "");
    }
    if (!n) buf_put(&b, "void");
    buf_put(&b, ")");
#undef SEP
    return b.d;
}

/* function-pointer type of a virtual slot: self is void* */
static char *slot_type(Routine *r, const char *name) {
    Buf b = {0};
    buf_printf(&b, "%s (*%s)(void*", ret_ctype(r), name ? name : "");
    if (r->kind == RK_DTOR) buf_put(&b, ", int");
    for (int i = 0; i < r->params.len; i++) {
        Param *p = &r->params.items[i];
        int ref = p->mode == PM_VAR || p->mode == PM_OUT || p->mode == PM_CONSTREF;
        buf_printf(&b, ", %s%s", ctype(p->type), ref ? "*" : "");
    }
    buf_put(&b, ")");
    return b.d;
}

static void gen_routine(Routine *r) {
    cur = r;
    level = r->level;
    loops.len = 0;
    Buf body = {0};
    out = &body;
    indent = 0;
    line("%s {", sig(r, 1));
    indent = 1;
    if (r->has_frame) {
        Buf f = {0};
        buf_printf(&f, "struct F%d {\n", r->id);
        if (r->level > 1) buf_printf(&f, "  struct F%d *__up;\n", r->parent->id);
        if (r->self) buf_printf(&f, "  struct S%d *self;\n", r->owner->id);
        for (int i = 0; i < r->params.len; i++) {
            Var *v = r->params.items[i].var;
            buf_printf(&f, "  %s%s %s;\n", ctype(v->type), v->is_ref ? "*" : "", v->cname);
        }
        if (r->result) buf_printf(&f, "  %s Result;\n", ctype(r->result->type));
        for (int i = 0; i < r->locals.len; i++) {
            Var *v = r->locals.items[i];
            buf_printf(&f, "  %s %s;\n", ctype(v->type), v->cname);
            if (v->is_stack) buf_printf(&f, "  struct S%d %s__st;\n  int %s__live;\n", v->type->st->id, v->cname, v->cname);
        }
        buf_put(&f, "  char __dummy;\n};\n");
        buf_put(&b_frames, buf_str(&f));
        line("struct F%d __f;", r->id);
        line("memset(&__f, 0, sizeof __f);");
        if (r->level > 1) line("__f.__up = __up;");
    }
    if (r->kind == RK_CTOR && r->owner->kind == SK_CLASS)
        line("if (__vmt) { self = (struct S%d*)__ppc_alloc(((struct __VMTHdr*)__vmt)->size); self->__vmt = __vmt; }",
             r->owner->id);
    if (r->has_frame) {
        if (r->self) line("__f.self = self;");
        for (int i = 0; i < r->params.len; i++) line("__f.%s = %s;", r->params.items[i].var->cname, r->params.items[i].var->cname);
    } else {
        if (r->result) line("%s Result; memset(&Result, 0, sizeof Result);", ctype(r->result->type));
        for (int i = 0; i < r->locals.len; i++) {
            Var *v = r->locals.items[i];
            line("%s %s; memset(&%s, 0, sizeof %s);", ctype(v->type), v->cname, v->cname, v->cname);
            if (v->is_stack)
                line("struct S%d %s__st; memset(&%s__st, 0, sizeof %s__st); int %s__live = 0;", v->type->st->id, v->cname,
                     v->cname, v->cname, v->cname);
        }
    }
    for (int i = 0; i < r->locals.len; i++) stack_init(r->locals.items[i]);
    gstmts(&r->body);
    line("goto __exit;");
    indent = 0;
    line("__exit:;");
    indent = 1;
    stack_destroy(r->locals.items, r->locals.len);
    for (int i = r->params.len - 1; i >= 0; i--)
        if (r->params.items[i].mode == PM_OWNED) stack_destroy(&r->params.items[i].var, 1);
    if (r->kind == RK_DTOR && r->owner->kind == SK_CLASS)
        line("if (__free) { ((struct __VMTHdr*)self->__vmt)->drop(self); __ppc_free(self); }");
    if (r->result) line("return %s;", var_ref(r->result));
    else if (r->kind == RK_CTOR && r->owner->kind == SK_CLASS) line("return self;");
    indent = 0;
    line("}");
    line("");
    buf_put(&b_funcs, buf_str(&body));
}

/* ============================================================== program */

void codegen(Program *p, FILE *f) {
    prog = p;
    if (p->tobject) {
        tobject_free = find_method(p->tobject, "Free");
        tobject_destroy = find_method(p->tobject, "Destroy");
    }

    for (int i = 0; i < p->structs.len; i++) buf_printf(&b_fwd, "struct S%d;\n", p->structs.items[i]->id);
    for (int i = 0; i < p->routines.len; i++)
        if (p->routines.items[i]->has_frame) buf_printf(&b_fwd, "struct F%d;\n", p->routines.items[i]->id);
    buf_put(&b_fwd, "struct __VMTHdr;\n");

    for (int i = 0; i < p->structs.len; i++) ensure_struct(p->structs.items[i]);

    /* VMTs */
    for (int i = 0; i < p->structs.len; i++) {
        Struct *s = p->structs.items[i];
        if (!s->has_vmt) continue;
        buf_printf(&b_vmtt, "struct VMT%d { /* %s */\n  struct __VMTHdr h;\n", s->id, s->name);
        for (int k = 0; k < s->slots.len; k++) buf_printf(&b_vmtt, "  %s;\n", slot_type(s->slots.items[k].intro, sfmt("v%d", k)));
        buf_put(&b_vmtt, "};\n");
        buf_printf(&b_globals, "extern struct VMT%d VMTI%d;\n", s->id, s->id);
        buf_printf(&b_vmti, "struct VMT%d VMTI%d = {\n  { %s, \"%s\", sizeof(struct S%d), D%d },\n", s->id, s->id,
                   s->parent && s->parent->has_vmt ? sfmt("&VMTI%d", s->parent->id) : "NULL", s->name, s->id, s->id);
        /* drop function: frees the fields that own an object (runs after Destroy) */
        buf_printf(&b_protos, "static void D%d(void *o);\n", s->id);
        buf_printf(&b_drops, "static void D%d(void *o) {\n  struct S%d *s = (struct S%d*)o; (void)s;\n", s->id, s->id, s->id);
        for (int k = 0; k < s->fields.len; k++) {
            Field *f = s->fields.items[k];
            if (!is_ref_type(f->type) || f->borrowed) continue;
            char *x = sfmt("s->%s", f->cname);
            buf_printf(&b_drops, "  if (%s) { %s; %s = 0; }\n", x, drop_code(f->type, x), x);
        }
        buf_put(&b_drops, "}\n");
        for (int k = 0; k < s->slots.len; k++) {
            Slot *sl = &s->slots.items[k];
            char *cast = slot_type(sl->intro, NULL);
            buf_printf(&b_vmti, "  (%s)%s,\n", cast, sl->impl->is_abstract ? "__abstract_err" : sl->impl->cname);
        }
        buf_put(&b_vmti, "};\n");
    }
    /* enum names */
    for (int i = 0; i < p->enums.len; i++) {
        EnumInfo *en = p->enums.items[i];
        buf_printf(&b_globals, "static const char *ENAMES%d[] = {", en->id);
        for (int k = 0; k < en->names.len; k++) buf_printf(&b_globals, "%s\"%s\"", k ? ", " : "", en->names.items[k]);
        buf_put(&b_globals, "};\n");
    }
    buf_printf(&b_globals, "static const char *__ppc_srcfile = %s;\n", c_strlit(g_filename, (int)strlen(g_filename)));
    /* globals */
    for (int i = 0; i < p->globals.len; i++) {
        Var *v = p->globals.items[i];
        buf_printf(&b_globals, "static %s %s;\n", ctype(v->type), v->cname);
        if (v->is_stack) buf_printf(&b_globals, "static struct S%d %s__st;\nstatic int %s__live;\n", v->type->st->id, v->cname, v->cname);
    }
    /* prototypes and bodies */
    for (int i = 0; i < p->routines.len; i++) {
        Routine *r = p->routines.items[i];
        if (r->is_abstract) continue;
        buf_printf(&b_protos, "static %s;\n", sig(r, 0));
    }
    for (int i = 0; i < p->routines.len; i++) {
        Routine *r = p->routines.items[i];
        if (r->is_abstract) continue;
        buf_put(&b_funcs, "static ");
        gen_routine(r);
    }
    /* main */
    Buf mb = {0};
    out = &mb;
    cur = NULL;
    level = 0;
    loops.len = 0;
    indent = 0;
    line("int main(int argc, char **argv) {");
    indent = 1;
    line("(void)argc; (void)argv;");
    for (int i = 0; i < p->globals.len; i++) stack_init(p->globals.items[i]);
    gstmts(&p->init);
    gstmts(&p->body);
    line("goto __exit;");
    indent = 0;
    line("__exit:;");
    indent = 1;
    stack_destroy(p->globals.items, p->globals.len);
    line("fflush(stdout);");
    line("return 0;");
    indent = 0;
    line("}");

    fprintf(f, "/* generated by ppc - Perfect Pascal Compiler; program %s */\n", p->name);
    fputs(PPC_RUNTIME, f);
    fputs("\n/* ---- forward declarations ---- */\n", f);
    fputs(buf_str(&b_fwd), f);
    fputs("\n/* ---- types ---- */\n", f);
    fputs(buf_str(&b_types), f);
    fputs("\n/* ---- virtual method tables ---- */\n", f);
    fputs(buf_str(&b_vmtt), f);
    fputs("\n/* ---- globals ---- */\n", f);
    fputs(buf_str(&b_globals), f);
    fputs("\n/* ---- prototypes ---- */\n", f);
    fputs(buf_str(&b_protos), f);
    fputs("\n", f);
    fputs(buf_str(&b_vmti), f);
    fputs("\n/* ---- frames for nested routines ---- */\n", f);
    fputs(buf_str(&b_frames), f);
    fputs("\n/* ---- routines ---- */\n", f);
    fputs(buf_str(&b_drops), f);
    fputs(buf_str(&b_funcs), f);
    fputs(buf_str(&mb), f);
}
