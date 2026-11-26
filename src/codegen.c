/* codegen.c - C code generation from the resolved AST */
#include "ast.h"
#include <ctype.h>
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
static VEC(int) loop_blk; /* open block statements when each loop started */
static VEC(Stmt *) cg_blocks; /* block statements being generated, innermost last */
static Routine *tobject_free, *tobject_destroy;

static char *gx(Expr *e);
static char *lit_elem(Expr *x, int own);
static char *ret_ctype(Routine *r);
static void gs(Stmt *s);

static char *ctype(Type *t);

/* runtime nil check around a reference-valued expression */
/* Only needed when the compile-time nil analysis is switched off (--no-nil-check):
 * with the analysis on, every dereferenced reference is proven non-nil. */
static char *nilchk(Expr *base, char *code, Pos p) {
    if (g_nil_check) return code;
    Expr *x = base;
    while (x->kind == E_CONV || x->kind == E_CAST) x = x->a;
    if (x->kind == E_VAR && x->var->vkind == VK_SELF) return code;
    if (x->kind == E_RCALL && x->rt->kind == RK_CTOR) return code;
    if (x->kind == E_ADDR) return code;
    return sfmt("((%s)__nilchk((void*)(%s), %d, %d))", ctype(base->type), code, p.line, p.col);
}

int g_line_dirs = 1;
const char *g_cfile = "out.c";
static Pos line_pos;    /* the Pascal statement being translated */
static int have_line_pos;
#define LINE_RESET "#line 0 \"@@ppc-reset@@\""
static void line_reset(void) { /* back to the lines of the C file (the writer puts in the real number) */
    have_line_pos = 0;
    if (g_line_dirs) buf_put(out, LINE_RESET "\n");
}
static void line(const char *fmt, ...) {
    if (g_line_dirs && have_line_pos) {
        const char *fn = POS_FILE(line_pos);
        if (fn && fn[0] != '<') { /* the built-in prelude has no file */
            buf_printf(out, "#line %d \"", line_pos.line);
            for (const char *c = fn; *c; c++) {
                if (*c == '"' || *c == '\\') buf_put(out, "\\");
                buf_putn(out, c, 1);
            }
            buf_put(out, "\"\n");
        }
    }
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
    case T_LIT: return "void";
    case T_PARAM: return "void"; /* placeholders never reach the C code */
    case T_SET: return "__Set";
    case T_CLASSREF: return "void*"; /* the VMT of the class */
    case T_PROC: { /* function value: a C function pointer */
        if (t->closure) return "__Closure";
        Routine *r = t->proc;
        Buf pb = {0};
        for (int i = 0; i < r->params.len; i++) {
            Param *p = &r->params.items[i];
            int ref = p->mode == PM_VAR || p->mode == PM_OUT || p->mode == PM_CONSTREF;
            buf_printf(&pb, "%s%s%s", i ? ", " : "", ctype(p->type), ref ? "*" : "");
        }
        char *key = sfmt("proc|%s|%s", ret_ctype(r), r->params.len ? buf_str(&pb) : "void");
        for (int i = 0; i < arrs.len; i++)
            if (STREQ(arrs.items[i].key, key)) return arrs.items[i].name;
        ArrDef d = {key, sfmt("P%d", arrs.len + 1)};
        vpush(arrs, d);
        buf_printf(&b_fwd, "typedef %s (*%s)(%s);\n", ret_ctype(r), d.name, r->params.len ? buf_str(&pb) : "void");
        return d.name;
    }
    case T_DYN: { /* dynamic array: length + buffer on the heap */
        char *elem = ctype(t->base);
        char *key = sfmt("dyn|%s", elem);
        for (int i = 0; i < arrs.len; i++)
            if (STREQ(arrs.items[i].key, key)) return sfmt("struct %s", arrs.items[i].name);
        if (t->base->kind == T_RECORD) ensure_struct(t->base->st);
        ArrDef d = {key, sfmt("D%d", arrs.len + 1)};
        vpush(arrs, d);
        buf_printf(&b_fwd, "struct %s;\n", d.name);
        buf_printf(&b_types, "struct %s { int64_t len; %s *a; };\n", d.name, elem);
        return sfmt("struct %s", d.name);
    }
    case T_RES: { /* result: struct { err; value; error } (zero = ok with the zero value) */
        char *elem = t->base ? ctype(t->base) : NULL, *et = ctype(t->err);
        char *key = sfmt("res|%s|%s", elem ? elem : "", et);
        for (int i = 0; i < arrs.len; i++)
            if (STREQ(arrs.items[i].key, key)) return sfmt("struct %s", arrs.items[i].name);
        if (t->base && t->base->kind == T_RECORD) ensure_struct(t->base->st);
        if (t->err->kind == T_RECORD) ensure_struct(t->err->st);
        ArrDef d = {key, sfmt("R%d", arrs.len + 1)};
        vpush(arrs, d);
        buf_printf(&b_fwd, "struct %s;\n", d.name);
        if (elem) buf_printf(&b_types, "struct %s { uint8_t err; %s v; %s e; };\n", d.name, elem, et);
        else buf_printf(&b_types, "struct %s { uint8_t err; %s e; };\n", d.name, et);
        return sfmt("struct %s", d.name);
    }
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

static char *agg_drop(Type *t, const char *lv);
static char *agg_copy(Type *t, const char *lv);
static char *agg_tmp(Type *t, const char *lv);
static int needs_drop(Type *t) { return has_owned(t) || has_str(t); }

/* function results that own objects must be stored: these are the places that take them */
static VEC(Expr *) taken_temps;
static void take_temp(Expr *e) {
    while ((e->kind == E_CONV || e->kind == E_OK || e->kind == E_ERR) && e->a) e = e->a;
    if (e->kind == E_RCALL && e->rt->ret_ref)
        error_at(e->pos, "the result of '%s' refers into its container and cannot be taken over: look at it with 'is some x', or take it out (e.g. with Extract or Take)",
                 e->rt->name);
    vpush(taken_temps, e);
    if (e->kind == E_LIST || e->kind == E_RECLIT)
        for (int i = 0; i < e->args.len; i++) take_temp(e->args.items[i]);
}
static int is_taken(Expr *e) {
    for (int i = 0; i < taken_temps.len; i++)
        if (taken_temps.items[i] == e) return 1;
    return 0;
}

/* code that frees an owned reference */
static char *drop_code(Type *t, const char *x) {
    if (t->kind == T_CLASS) return sfmt("%s((struct S%d*)(%s))", tobject_free->cname, tobject_free->owner->id, x);
    if (t->kind == T_PTR && needs_drop(t->base)) /* heap value that owns objects or strings: free them first */
        return sfmt("(%s, __ppc_free(%s))", agg_drop(t->base, sfmt("*(%s)", x)), x);
    return sfmt("__ppc_free(%s)", x);
}

/* arrays, records and optional values that own objects: a drop function per type */
static VEC(char *) adrop_keys;
static VEC(char *) adrop_names;
static char *agg_drop_fn(Type *t) {
    char *ct = ctype(t);
    for (int i = 0; i < adrop_keys.len; i++)
        if (STREQ(adrop_keys.items[i], ct)) return adrop_names.items[i];
    char *name = sfmt("X%d", adrop_keys.len + 1);
    vpush(adrop_keys, ct);
    vpush(adrop_names, name);
    buf_printf(&b_protos, "static void %s(%s *p);\n", name, ct);
    Buf b = {0};
    buf_printf(&b, "static void %s(%s *p) {\n", name, ct);
    if (t->kind == T_STR) {
        buf_put(&b, "  __pstr_free(p);\n");
    } else if (t->kind == T_RES) {
        if (t->base && is_ref_type(t->base)) buf_printf(&b, "  if (!p->err && p->v) { %s; p->v = 0; }\n", drop_code(t->base, "p->v"));
        else if (t->base && needs_drop(t->base)) buf_printf(&b, "  if (!p->err) %s;\n", agg_drop(t->base, "p->v"));
        if (needs_drop(t->err)) buf_printf(&b, "  if (p->err) %s;\n", agg_drop(t->err, "p->e"));
    } else if (t->kind == T_DYN) { /* elements, then the buffer */
        if (is_ref_type(t->base) || needs_drop(t->base)) {
            buf_put(&b, "  for (int64_t i = 0; i < p->len; i++) {\n");
            if (is_ref_type(t->base)) buf_printf(&b, "    if (p->a[i]) { %s; p->a[i] = 0; }\n", drop_code(t->base, "p->a[i]"));
            else buf_printf(&b, "    %s;\n", agg_drop(t->base, "p->a[i]"));
            buf_put(&b, "  }\n");
        }
        buf_put(&b, "  __ppc_free(p->a); p->a = 0; p->len = 0;\n");
    } else if (t->kind == T_ARRAY) {
        long long n = t->hi - t->lo + 1;
        buf_printf(&b, "  for (long i = 0; i < %lld; i++) {\n", n);
        if (is_ref_type(t->base))
            buf_printf(&b, "    if (p->a[i]) { %s; p->a[i] = 0; }\n", drop_code(t->base, "p->a[i]"));
        else
            buf_printf(&b, "    %s;\n", agg_drop(t->base, "p->a[i]"));
        buf_put(&b, "  }\n");
    } else if (t->kind == T_OPT) {
        buf_printf(&b, "  if (p->has) { %s; p->has = 0; }\n", agg_drop(t->base, "p->v"));
    } else {
        for (int k = 0; k < t->st->fields.len; k++) {
            Field *f = t->st->fields.items[k];
            char *x = sfmt("p->%s", f->cname);
            if (is_ref_type(f->type) && !f->borrowed) buf_printf(&b, "  if (%s) { %s; %s = 0; }\n", x, drop_code(f->type, x), x);
            else if (needs_drop(f->type)) buf_printf(&b, "  %s;\n", agg_drop(f->type, x));
        }
    }
    buf_put(&b, "}\n");
    buf_put(&b_drops, buf_str(&b));
    return name;
}
static char *agg_drop(Type *t, const char *lv) { return sfmt("%s(&(%s))", agg_drop_fn(t), lv); }

/* Hash(x): one function per record or array type, combining its parts */
static VEC(char *) ahash_keys;
static VEC(char *) ahash_names;
static char *hash_of(Type *t, const char *x) {
    switch (t->kind) {
    case T_STR: return sfmt("__hash_str(%s)", x);
    case T_REAL: return sfmt("__hash_real(%s)", x);
    case T_SET: return sfmt("__hash_set(%s)", x);
    case T_RECORD: case T_ARRAY: {
        char *ct = ctype(t), *name = NULL;
        for (int i = 0; i < ahash_keys.len; i++)
            if (STREQ(ahash_keys.items[i], ct)) name = ahash_names.items[i];
        if (!name) {
            name = sfmt("H%d", ahash_keys.len + 1);
            vpush(ahash_keys, ct);
            vpush(ahash_names, name);
            buf_printf(&b_protos, "static int64_t %s(%s *a);\n", name, ct);
            Buf b = {0};
            buf_printf(&b, "static int64_t %s(%s *a) {\n  int64_t h = 0;\n", name, ct);
            if (t->kind == T_RECORD)
                for (int k = 0; k < t->st->fields.len; k++)
                    buf_printf(&b, "  h = __hash_comb(h, %s);\n", hash_of(t->st->fields.items[k]->type, sfmt("a->%s", t->st->fields.items[k]->cname)));
            else
                buf_printf(&b, "  for (long i = 0; i < %lld; i++) h = __hash_comb(h, %s);\n", t->hi - t->lo + 1, hash_of(t->base, "a->a[i]"));
            buf_put(&b, "  return h;\n}\n");
            buf_put(&b_drops, buf_str(&b));
        }
        int id = ++tmpc;
        return sfmt("({ %s __hv%d = %s; %s(&__hv%d); })", ct, id, x, name, id);
    }
    default: return sfmt("__hash_mix((uint64_t)(int64_t)(%s))", x); /* numbers, Char, Boolean, enumerations */
    }
}

/* '=' of records, arrays and optional values: one function per type */
static VEC(char *) aeq_keys;
static VEC(char *) aeq_names;
static char *agg_eq_fn(Type *t);
static char *elem_eq(Type *t, const char *x, const char *y) {
    switch (t->kind) {
    case T_STR: return sfmt("__pstr_cmp(%s, %s) == 0", x, y);
    case T_RECORD: case T_ARRAY: case T_DYN: case T_OPT: return sfmt("%s(&(%s), &(%s))", agg_eq_fn(t), x, y);
    default: return sfmt("(%s) == (%s)", x, y); /* numbers, enumerations, objects (the same object) */
    }
}
static char *agg_eq_fn(Type *t) {
    char *ct = ctype(t);
    for (int i = 0; i < aeq_keys.len; i++)
        if (STREQ(aeq_keys.items[i], ct)) return aeq_names.items[i];
    char *name = sfmt("Q%d", aeq_keys.len + 1);
    vpush(aeq_keys, ct);
    vpush(aeq_names, name);
    buf_printf(&b_protos, "static int %s(%s *a, %s *b);\n", name, ct, ct);
    Buf b = {0};
    buf_printf(&b, "static int %s(%s *a, %s *b) {\n", name, ct, ct);
    if (t->kind == T_RECORD) {
        for (int k = 0; k < t->st->fields.len; k++) {
            Field *f = t->st->fields.items[k];
            buf_printf(&b, "  if (!(%s)) return 0;\n", elem_eq(f->type, sfmt("a->%s", f->cname), sfmt("b->%s", f->cname)));
        }
    } else if (t->kind == T_ARRAY) {
        buf_printf(&b, "  for (long i = 0; i < %lld; i++) if (!(%s)) return 0;\n", t->hi - t->lo + 1, elem_eq(t->base, "a->a[i]", "b->a[i]"));
    } else if (t->kind == T_DYN) {
        buf_put(&b, "  if (a->len != b->len) return 0;\n");
        buf_printf(&b, "  for (int64_t i = 0; i < a->len; i++) if (!(%s)) return 0;\n", elem_eq(t->base, "a->a[i]", "b->a[i]"));
    } else if (t->kind == T_OPT) {
        buf_printf(&b, "  if (a->has != b->has) return 0;\n  if (!a->has) return 1;\n  return %s;\n", elem_eq(t->base, "a->v", "b->v"));
    }
    buf_put(&b, "  return 1;\n}\n");
    buf_put(&b_drops, buf_str(&b));
    return name;
}

/* per value type with strings: kind 0 = give a shallow copy its own string buffers (copy on
 * assignment), kind 1 = hand its buffers to the temporary pool (function results) */
static VEC(char *) astr_keys[2];
static VEC(char *) astr_names[2];
static char *agg_str_fn(Type *t, int kind) {
    char *ct = ctype(t);
    for (int i = 0; i < astr_keys[kind].len; i++)
        if (STREQ(astr_keys[kind].items[i], ct)) return astr_names[kind].items[i];
    char *name = sfmt("%s%d", kind ? "Z" : "W", astr_keys[kind].len + 1);
    vpush(astr_keys[kind], ct);
    vpush(astr_names[kind], name);
    buf_printf(&b_protos, "static void %s(%s *p);\n", name, ct);
    Buf b = {0};
    buf_printf(&b, "static void %s(%s *p) {\n", name, ct);
#define ELEM(T, X) (((T)->kind == T_STR) ? (kind ? sfmt("__ppc_tmp(%s)", X) : sfmt("%s = __pstr_dup(%s)", X, X)) : sfmt("%s(&(%s))", agg_str_fn(T, kind), X))
    if (t->kind == T_STR) buf_put(&b, kind ? "  __ppc_tmp(*p);\n" : "  *p = __pstr_dup(*p);\n");
    else if (t->kind == T_ARRAY) {
        buf_printf(&b, "  for (long i = 0; i < %lld; i++) %s;\n", t->hi - t->lo + 1, ELEM(t->base, "p->a[i]"));
    } else if (t->kind == T_DYN) {
        buf_printf(&b, "  for (int64_t i = 0; i < p->len; i++) %s;\n", ELEM(t->base, "p->a[i]"));
    } else if (t->kind == T_OPT) {
        buf_printf(&b, "  if (p->has) %s;\n", ELEM(t->base, "p->v"));
    } else if (t->kind == T_RES) {
        if (t->base && has_str(t->base)) buf_printf(&b, "  if (!p->err) %s;\n", ELEM(t->base, "p->v"));
        if (has_str(t->err)) buf_printf(&b, "  if (p->err) %s;\n", ELEM(t->err, "p->e"));
    } else
        for (int k = 0; k < t->st->fields.len; k++) {
            Field *f = t->st->fields.items[k];
            if (has_str(f->type)) buf_printf(&b, "  %s;\n", ELEM(f->type, sfmt("p->%s", f->cname)));
        }
#undef ELEM
    buf_put(&b, "}\n");
    buf_put(&b_drops, buf_str(&b));
    return name;
}
static char *agg_copy(Type *t, const char *lv) { return sfmt("%s(&(%s))", agg_str_fn(t, 0), lv); }
static char *agg_tmp(Type *t, const char *lv) { return sfmt("%s(&(%s))", agg_str_fn(t, 1), lv); }

static char *sig(Routine *r, int names);
static char *ret_ctype(Routine *r);

/* ---- generated Clone ('function Clone: T; const; auto;'): deep copy ---- */

static Routine *find_clone(Struct *st) {
    for (Struct *c = st; c; c = c->parent)
        for (int i = 0; i < c->methods.len; i++) {
            Routine *m = c->methods.items[i];
            if (STREQ(m->name, "Clone") && m->kind == RK_FUNC && !m->params.len && !m->is_classm) return m;
        }
    return NULL;
}

/* a class whose objects can be cloned by a generated Clone (it or an ancestor has one) */
static int needs_clone(Struct *s) {
    for (Struct *c = s; c; c = c->parent)
        for (int i = 0; i < c->methods.len; i++)
            if (c->methods.items[i]->is_auto) return 1;
    return 0;
}

static Pos clone_pos;
static const char *clone_what;
/* expression: a clone of the object x (class type t) */
static char *clone_call(Type *t, const char *x, const char *field) {
    Routine *m = find_clone(t->st);
    if (!m)
        error_at(clone_pos, "%s cannot be cloned: %s owns a \"%s\", which has no Clone (declare 'function Clone: %s; const; auto;' there)",
                 clone_what, STREQ(field, "[]") ? "each element" : sfmt("its field '%s'", field), t->st->name, t->st->name);
    char *call = m->slot >= 0 ? sfmt("((struct VMT%d*)((%s)->__vmt))->v%d(%s)", t->st->id, x, m->slot, x)
                              : sfmt("%s((struct S%d*)(%s))", m->cname, m->owner->id, x);
    if (m->ret->st != t->st) call = sfmt("((struct S%d*)__as((void*)(%s), &VMTI%d))", t->st->id, call, t->st->id);
    return call;
}

static char *agg_clone(Type *t, const char *lv);
static VEC(char *) aclone_keys;
static VEC(char *) aclone_names;
/* per value type that owns objects: replaces the owned objects of a fresh copy by clones */
static char *agg_clone_fn(Type *t) {
    char *ct = ctype(t);
    for (int i = 0; i < aclone_keys.len; i++)
        if (STREQ(aclone_keys.items[i], ct)) return aclone_names.items[i];
    char *name = sfmt("Y%d", aclone_keys.len + 1);
    vpush(aclone_keys, ct);
    vpush(aclone_names, name);
    buf_printf(&b_protos, "static void %s(%s *p);\n", name, ct);
    Buf b = {0};
    buf_printf(&b, "static void %s(%s *p) {\n", name, ct);
    if (t->kind == T_DYN) { /* own buffer for the copy, then clone what the elements own */
        char *et = ctype(t->base);
        buf_printf(&b, "  if (p->len) { %s *n = (%s*)__ppc_alloc((size_t)p->len * sizeof(%s)); memcpy(n, p->a, (size_t)p->len * sizeof(%s)); p->a = n; }\n",
                   et, et, et, et);
        if (is_ref_type(t->base) || needs_drop(t->base)) {
            buf_put(&b, "  for (int64_t i = 0; i < p->len; i++) {\n");
            if (is_ref_type(t->base)) buf_printf(&b, "    if (p->a[i]) p->a[i] = %s;\n", clone_call(t->base, "p->a[i]", "[]"));
            else if (has_owned(t->base)) buf_printf(&b, "    %s;\n", agg_clone(t->base, "p->a[i]"));
            else buf_printf(&b, "    %s;\n", agg_copy(t->base, "p->a[i]"));
            buf_put(&b, "  }\n");
        }
    } else if (t->kind == T_ARRAY) {
        buf_printf(&b, "  for (long i = 0; i < %lld; i++) {\n", t->hi - t->lo + 1);
        if (is_ref_type(t->base)) buf_printf(&b, "    if (p->a[i]) p->a[i] = %s;\n", clone_call(t->base, "p->a[i]", "[]"));
        else if (has_owned(t->base)) buf_printf(&b, "    %s;\n", agg_clone(t->base, "p->a[i]"));
        else if (has_str(t->base)) buf_printf(&b, "    %s;\n", agg_copy(t->base, "p->a[i]"));
        buf_put(&b, "  }\n");
    } else if (t->kind == T_OPT) {
        buf_printf(&b, "  if (p->has) %s;\n", has_owned(t->base) ? agg_clone(t->base, "p->v") : agg_copy(t->base, "p->v"));
    } else {
        for (int k = 0; k < t->st->fields.len; k++) {
            Field *f = t->st->fields.items[k];
            char *x = sfmt("p->%s", f->cname);
            if (is_ref_type(f->type) && !f->borrowed) buf_printf(&b, "  if (%s) %s = %s;\n", x, x, clone_call(f->type, x, f->name));
            else if (has_owned(f->type)) buf_printf(&b, "  %s;\n", agg_clone(f->type, x));
            else if (has_str(f->type)) buf_printf(&b, "  %s;\n", agg_copy(f->type, x));
        }
    }
    buf_put(&b, "}\n");
    buf_put(&b_drops, buf_str(&b));
    return name;
}
static char *agg_clone(Type *t, const char *lv) { return sfmt("%s(&(%s))", agg_clone_fn(t), lv); }

/* K<id>: fix-up after an object of class s was copied byte for byte */
static void gen_clonefix(Struct *s) {
    clone_pos = s->pos;
    clone_what = sfmt("\"%s\"", s->name);
    buf_printf(&b_protos, "static void K%d(void *o);\n", s->id);
    Buf b = {0};
    buf_printf(&b, "static void K%d(void *o) {\n  struct S%d *s = (struct S%d*)o; (void)s;\n", s->id, s->id, s->id);
    for (int k = 0; k < s->fields.len; k++) {
        Field *f = s->fields.items[k];
        char *x = sfmt("s->%s", f->cname);
        if (is_ref_type(f->type) && !f->borrowed) buf_printf(&b, "  if (%s) %s = %s;\n", x, x, clone_call(f->type, x, f->name));
        else if (has_owned(f->type)) buf_printf(&b, "  %s;\n", agg_clone(f->type, x));
        else if (has_str(f->type)) buf_printf(&b, "  %s;\n", agg_copy(f->type, x));
    }
    buf_put(&b, "}\n");
    buf_put(&b_drops, buf_str(&b));
}

/* body of a generated Clone */
static void gen_auto_clone(Routine *r) {
    Struct *s = r->owner;
    buf_printf(&b_protos, "static %s;\n", sig(r, 0));
    Buf b = {0};
    buf_printf(&b, "static %s {\n", sig(r, 1));
    if (s->kind == SK_CLASS) { /* copy the whole (dynamic) object, then clone what it owns */
        buf_put(&b, "  struct __VMTHdr *h = *(struct __VMTHdr **)self;\n");
        buf_put(&b, "  void *n = __ppc_alloc(h->size);\n  memcpy(n, self, h->size);\n  h->clonefix(n);\n");
        buf_printf(&b, "  return (%s)n;\n", ret_ctype(r));
    } else {
        clone_pos = s->pos;
        clone_what = sfmt("\"%s\"", s->name);
        buf_printf(&b, "  struct S%d r = *self;\n", s->id);
        if (has_owned(s->type)) buf_printf(&b, "  %s;\n", agg_clone(s->type, "r"));
        else if (has_str(s->type)) buf_printf(&b, "  %s;\n", agg_copy(s->type, "r"));
        buf_put(&b, "  return r;\n");
    }
    buf_put(&b, "}\n");
    buf_put(&b_funcs, buf_str(&b));
}

/* a stack object given to an owner: it moves to the heap (like Box::new in Rust) */
static Var *stack_var_of(Expr *e) {
    while (e->kind == E_CONV || e->kind == E_CAST) e = e->a;
    return e->kind == E_VAR && e->var->is_stack ? e->var : NULL;
}
static char *box_move(Var *k, Type *to) {
    int id = ++tmpc;
    int sid = k->type->st->id;
    return sfmt("((%s)({ void *__m%d = __ppc_alloc(sizeof(struct S%d)); memcpy(__m%d, %s, sizeof(struct S%d)); %s = 0; __m%d; }))",
                ctype(to), id, sid, id, var_raw(k), sid, var_aux(k, "__live"), id);
}

/* value variable that owns the objects inside it */
static int agg_owner_var(Var *v) {
    return needs_drop(v->type) && !v->is_ref && !v->no_drop &&
           (v->vkind == VK_LOCAL || v->vkind == VK_GLOBAL || (v->vkind == VK_PARAM && v->mode == PM_OWNED));
}

/* variable that owns what it refers to (and frees it at the end of its scope) */
/* owning bindings made by the condition of an 'if' ('if Open(n) is ok f'): freed when the 'if' ends */
static void cond_owners(Expr *c, VarList *out) {
    if (!c) return;
    if (c->kind == E_BINARY && c->op == OP_AND) {
        cond_owners(c->a, out);
        cond_owners(c->b, out);
    } else if (c->kind == E_ISSOME && c->var && is_ref_type(c->var->type) && !c->var->borrowed && !c->var->is_stack)
        vpush(*out, c->var);
    else if (c->kind == E_ISSOME && c->var && !is_ref_type(c->var->type) && has_owned(c->var->type) && !c->var->no_drop)
        vpush(*out, c->var); /* a value that owns objects, taken from a fresh value */
}
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
    if (e->kind == E_VAR) return e->var->vkind != VK_SELF && !e->var->is_stack;
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
        } else if (pm->mode == PM_OWNED && stack_var_of(a)) {
            buf_put(&args, box_move(stack_var_of(a), pm->type));
        } else if (pm->mode == PM_OWNED && has_owned(pm->type)) { /* a value that owns objects moves into the callee */
            Expr *src = a;
            while (src->kind == E_CONV && src->a) src = src->a;
            if (cg_lvalue(src)) {
                int id = ++tmpc;
                char *sx = gx(src);
                buf_printf(&args, "({ %s __mv%d = %s; memset(&(%s), 0, sizeof(%s)); __mv%d; })", ctype(pm->type), id, gx(a), sx, sx, id);
            } else {
                take_temp(a);
                buf_put(&args, gx(a));
            }
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

static char *slot_type(Routine *r, const char *name);
static char *gcall_core(Expr *e, const char *selfp, Struct *sst, const char *as) {
    Routine *r = e->rt;
    const char *dflag = "";
    switch (e->cmode) {
    case CM_INDIRECT:
        if (e->b->type->closure) { /* with a frame: the frame is the first argument; without: a plain routine */
            Type plain = *e->b->type;
            plain.closure = 0;
            int id = ++tmpc;
            return sfmt("({ __Closure __c%d = %s; __c%d.env ? ((%s)__c%d.fn)(__c%d.env%s) : ((%s)__c%d.fn)(%s); })", id, gx(e->b), id,
                        slot_type(r, NULL), id, id, as, ctype(&plain), id, as[0] ? as + 2 : "");
        }
        return sfmt("(%s)(%s)", gx(e->b), as[0] ? as + 2 : "");
    case CM_NORMAL:
        if (r->level > 1) return sfmt("%s(%s%s)", r->cname, frame_ptr(r->level - 1), as);
        return sfmt("%s(%s)", r->cname, as[0] ? as + 2 : "");
    case CM_NEWCLASS:
        if (e->b) { /* c.Create(...): an object of the class in c; a virtual constructor in that class's version */
            int id = ++tmpc;
            if (r->slot >= 0)
                return sfmt("({ void *__cr%d = __nilref(%s); ((%s)((%s)((struct VMT%d*)__cr%d)->v%d)(NULL, __cr%d%s)); })", id, gx(e->b),
                            ctype(e->ctype), slot_type(r, NULL), e->ctype->st->id, id, r->slot, id, as);
            return sfmt("({ void *__cr%d = __nilref(%s); ((%s)%s(NULL, __cr%d%s)); })", id, gx(e->b), ctype(e->ctype), r->cname, id, as);
        }
        return sfmt("((%s)%s(NULL, &VMTI%d%s))", ctype(e->ctype), r->cname, e->ctype->st->id, as);
    case CM_CTOR_INST: {
        char *call = sfmt("%s((struct S%d*)%s, NULL%s)", r->cname, r->owner->id, selfp, as);
        if (e->a->kind == E_VAR && e->a->var->is_stack) /* stack object: construct in place */
            return sfmt("(%s, %s = 1, %s)", call, var_aux(e->a->var, "__live"), selfp);
        return call;
    }
    case CM_IFACE: /* the method from the object's table for the interface */
        return sfmt("((%s)__itab((void*)(%s), &IID%d)[%d])((void*)(%s)%s)", slot_type(r, NULL), selfp, r->owner->id, r->slot, selfp, as);
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
            if (a->type->kind == T_STR) buf_printf(&b, "__pstr_assign(&(%s), %s)", gx(a), f);
            else buf_printf(&b, "(%s = (%s)%s)", gx(a), ctype(a->type), f);
        }
        if (e->bi == BI_READLN) buf_put(&b, A->len ? ", __r_ln()" : "__r_ln()");
        else if (!A->len) buf_put(&b, "(void)0");
        buf_put(&b, ")");
        return b.d;
    }
    case BI_LENGTH:
        if (A->items[0]->type->kind == T_DYN) return sfmt("((int32_t)(%s).len)", G(0));
        if (A->items[0]->type->kind == T_CHAR) return "1";
        return sfmt("((int32_t)(%s).len)", G(0));
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
    case BI_HASH: return hash_of(A->items[0]->type, G(0));
    case BI_TOUCH: return "1";
    case BI_INCLUDE: return sfmt("__set_incl(&(%s), (int64_t)(%s), %lld, %lld)", G(0), G(1), A->items[0]->type->lo, A->items[0]->type->hi);
    case BI_EXCLUDE: return sfmt("__set_excl(&(%s), (int64_t)(%s), %lld, %lld)", G(0), G(1), A->items[0]->type->lo, A->items[0]->type->hi);
    case BI_FORMAT: {
        if (A->len == 1) return sfmt("__format(%s, 0, NULL)", G(0));
        Buf b = {0};
        for (int i = 1; i < A->len; i++) {
            Expr *x = A->items[i];
            Type *t = x->type;
            char *v = gx(x);
            if (t->kind == T_REAL) buf_printf(&b, "{'r', 0, %s, {0, 0}}, ", v);
            else if (t->kind == T_STR) buf_printf(&b, "{'s', 0, 0, %s}, ", v);
            else if (t->kind == T_CHAR) buf_printf(&b, "{'s', 0, 0, __pstr_char(%s)}, ", v);
            else if (t->kind == T_BOOL) buf_printf(&b, "{'s', 0, 0, __pstr_cstr((%s) ? \"TRUE\" : \"FALSE\")}, ", v);
            else if (t->kind == T_ENUM) buf_printf(&b, "{'s', 0, 0, __pstr_cstr(ENAMES%d[%s])}, ", t->en->id, v);
            else buf_printf(&b, "{'i', (int64_t)(%s), 0, {0, 0}}, ", v);
        }
        return sfmt("__format(%s, %d, (__FArg[]){%s})", G(0), A->len - 1, buf_str(&b));
    }
    case BI_STR: {
        Expr *a = A->items[0];
        Expr *v = a->kind == E_FMT ? a->a : a;
        const char *w = a->kind == E_FMT ? gx(a->b) : "0";
        const char *p = a->kind == E_FMT && a->c ? gx(a->c) : "-1";
        if (v->type->kind == T_REAL) return sfmt("__pstr_assign(&(%s), __str_real(%s, %s, %s))", G(1), gx(v), w, p);
        return sfmt("__pstr_assign(&(%s), __str_int(%s, %s))", G(1), gx(v), w);
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
    case BI_ARRCLONE: { /* copy of the array; owned objects are cloned */
        if (!has_owned(e->type)) return G(0);
        int id = ++tmpc;
        clone_pos = e->pos;
        clone_what = sfmt("\"%s\"", type_name(e->type));
        return sfmt("({ %s __c%d = %s; %s; __c%d; })", ctype(e->type), id, G(0), agg_clone(e->type, sfmt("__c%d", id)), id);
    }
    case BI_ISOK: return sfmt("(!(%s).err)", G(0));
    case BI_ISERROR: return sfmt("((%s).err != 0)", G(0));
    case BI_VALUEOR: {
        int id = ++tmpc;
        if (A->items[0]->type->kind == T_RES)
            return sfmt("({ %s __o%d = %s; __o%d.err ? (%s) : __o%d.v; })", ctype(A->items[0]->type), id, G(0), id, G(1), id);
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
        if (e->type->kind == T_OPT) return sfmt("({ %s __t%d = %s; (%s).has = 0; __t%d; })", ctype(e->type), id, l, l, id);
        return sfmt("({ %s __t%d = %s; %s = 0; __t%d; })", ctype(e->type), id, l, l, id);
    }
    case BI_SETLENGTH: {
        Type *t = A->items[0]->type;
        if (t->kind != T_DYN) return sfmt("__pstr_setlen(&(%s), (int64_t)(%s))", G(0), G(1));
        /* dynamic array: elements cut off are freed (if they own something), new ones start at zero */
        int id = ++tmpc;
        char *et = ctype(t->base);
        Buf b = {0};
        buf_printf(&b, "({ %s *__d%d = &(%s); int64_t __n%d = (int64_t)(%s); if (__n%d < 0) __n%d = 0; ", ctype(t), id, G(0), id, G(1), id, id);
        if (is_ref_type(t->base) || needs_drop(t->base)) {
            char *x = sfmt("__d%d->a[__k%d]", id, id);
            buf_printf(&b, "for (int64_t __k%d = __n%d; __k%d < __d%d->len; __k%d++) { ", id, id, id, id, id);
            if (is_ref_type(t->base)) buf_printf(&b, "if (%s) %s; ", x, drop_code(t->base, x));
            else buf_printf(&b, "%s; ", agg_drop(t->base, x));
            buf_put(&b, "} ");
        }
        buf_printf(&b, "__d%d->a = (%s*)__ppc_realloc(__d%d->a, (size_t)__n%d * sizeof(%s)); ", id, et, id, id, et);
        buf_printf(&b, "if (__n%d > __d%d->len) memset(__d%d->a + __d%d->len, 0, (size_t)(__n%d - __d%d->len) * sizeof(%s)); ", id, id,
                   id, id, id, id, et);
        buf_printf(&b, "__d%d->len = __n%d; (void)0; })", id, id);
        return b.d;
    }
    case BI_HIGH: return sfmt("((int32_t)(%s).len - 1)", G(0));
    case BI_CLASSNAME: return sfmt("__pstr_cstr((*(struct __VMTHdr**)(%s))->name)", G(0));
    case BI_REFNAME: return sfmt("__pstr_cstr(((struct __VMTHdr*)__nilref(%s))->name)", G(0));
    case BI_CLASSTYPE: return sfmt("(*(void**)(%s))", G(0));
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
        if (a->type->kind == T_DYN) { /* a[i]: 0 .. Length(a) - 1 */
            if (!g_range_check) return sfmt("(%s).a[%s]", gx(a), idx);
            int id = ++tmpc;
            Pos p = e->args.items[0]->pos;
            return sfmt("(*({ %s *__d%d = &(%s); int64_t __i%d = (int64_t)(%s); __rangechk(__i%d, 0, __d%d->len - 1, %d, %d); &__d%d->a[__i%d]; }))",
                        ctype(a->type), id, gx(a), id, idx, id, id, p.line, p.col, id, id);
        }
        if (a->type->kind == T_STR) { /* s[i]: 1 .. Length(s) */
            int id = ++tmpc;
            Pos p = e->args.items[0]->pos;
            char *chk = g_range_check ? sfmt("__rangechk(__i%d, 1, __d%d%slen, %d, %d); ", id, id, cg_lvalue(a) ? "->" : ".", p.line, p.col) : "";
            if (cg_lvalue(a))
                return sfmt("(*({ PStr *__d%d = &(%s); int64_t __i%d = (int64_t)(%s); %s&__d%d->p[__i%d - 1]; }))", id, gx(a), id, idx, chk,
                            id, id);
            return sfmt("({ PStr __d%d = %s; int64_t __i%d = (int64_t)(%s); %s__d%d.p[__i%d - 1]; })", id, gx(a), id, idx, chk, id, id);
        }
        if (g_range_check && !e->in_range) { /* index checked at run time (Runtime error 201) */
            Pos p = e->args.items[0]->pos;
            if (0) idx = "";
            else
                idx = sfmt("__rangechk((int64_t)(%s), %s, %s, %d, %d)", idx, int_lit(a->type->lo), int_lit(a->type->hi), p.line,
                           p.col);
        }
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
        if (ta->kind == T_SET) {
            switch (op) {
            case OP_ADD: return sfmt("__set_or(%s, %s)", a, b);
            case OP_SUB: return sfmt("__set_sub(%s, %s)", a, b);
            case OP_MUL: return sfmt("__set_and(%s, %s)", a, b);
            case OP_EQ: return sfmt("__set_eq(%s, %s)", a, b);
            case OP_NE: return sfmt("(!__set_eq(%s, %s))", a, b);
            case OP_LE: return sfmt("__set_le(%s, %s)", a, b);
            case OP_GE: return sfmt("__set_le(%s, %s)", b, a);
            }
        }
        if (ta->kind == T_STR) {
            if (op == OP_ADD) return sfmt("__pstr_cat(%s, %s)", a, b);
            return sfmt("(__pstr_cmp(%s, %s) %s 0)", a, b, binop_str(op));
        }
        if ((op == OP_EQ || op == OP_NE) &&
            (ta->kind == T_RECORD || ta->kind == T_ARRAY || ta->kind == T_DYN || (ta->kind == T_OPT && e->b->type->kind == T_OPT))) {
            int id = ++tmpc; /* field by field (through copies: the operands may be computed values) */
            return sfmt("(%s({ %s __ea%d = %s; %s __eb%d = %s; %s(&__ea%d, &__eb%d); }))", op == OP_NE ? "!" : "", ctype(ta), id, a,
                        ctype(ta), id, b, agg_eq_fn(ta), id, id);
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
        if (e->b->kind != E_SET || e->b->type->kind == T_SET) /* x in s */
            return sfmt("__set_has(%s, (int64_t)(%s), %lld, %lld)", gx(e->b), gx(e->a), e->b->type->lo, e->b->type->hi);
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
    case E_IS:
        if (e->b) return sfmt("__is((void*)(%s), %s)", gx(e->a), gx(e->b)); /* obj is c */
        if (e->ctype->st->is_iface) return sfmt("__implements((void*)(%s), &IID%d)", gx(e->a), e->ctype->st->id);
        return sfmt("__is((void*)(%s), &VMTI%d)", gx(e->a), e->ctype->st->id);
    case E_AS:
        if (e->b) return sfmt("((struct S%d*)__as((void*)(%s), %s))", e->ctype->st->id, gx(e->a), gx(e->b));
        if (e->ctype->st->is_iface)
            return sfmt("((struct S%d*)__as_iface((void*)(%s), &IID%d))", e->ctype->st->id, gx(e->a), e->ctype->st->id);
        return sfmt("((struct S%d*)__as((void*)(%s), &VMTI%d))", e->ctype->st->id, gx(e->a), e->ctype->st->id);
    case E_OK: /* a result that is moved (it owns objects) needs its own strings */
        if (e->a) /* an owned payload moves (gassign empties its source); strings are copied */
            return sfmt("((%s){.err = 0, .v = %s})", ctype(e->type), has_owned(e->a->type) ? gx(e->a) : lit_elem(e->a, has_owned(e->type)));
        return sfmt("((%s){.err = 0})", ctype(e->type));
    case E_ERR: return sfmt("((%s){.err = 1, .e = %s})", ctype(e->type), lit_elem(e->a, has_owned(e->type)));
    case E_TRY: { /* failed: the function returns the error; else the value */
        take_temp(e->a);
        int id = ++tmpc;
        Type *rt = e->a->type;
        char *res = var_ref(cur->result);
        return sfmt("({ %s __t%d = %s; if (__t%d.err) { %s.err = 1; %s.e = __t%d.e; %s goto __exit; } %s; })", ctype(rt), id,
                    gx(e->a), id, res, res, id, has_str(rt->err) && !has_owned(rt) ? sfmt("%s;", agg_copy(rt->err, sfmt("%s.e", res))) : "",
                    rt->base ? sfmt("__t%d.v", id) : "(void)0");
    }
    case E_COND: return sfmt("(%s ? %s : %s)", gx(e->a), gx(e->b), gx(e->c));
    case E_CASEX: /* case expression: the selector (when computed) is stored first */
        if (e->var) return sfmt("({ %s = %s; %s; })", var_ref(e->var), gx(e->a), gx(e->b));
        return gx(e->b);
    case E_DEFAULT: /* zero value of the target type */
        if (e->type->kind == T_CLASS) return "((void*)0)";
        return sfmt("((%s){0})", ctype(e->type));
    case E_SET: { /* [Red, Blue..Green]: a set value */
        Type *t = e->type;
        int id = ++tmpc;
        Buf b = {0};
        buf_printf(&b, "({ __Set __st%d = {{0, 0, 0, 0}}; ", id);
        for (int i = 0; i < e->args.len; i++) {
            if (e->args2.items[i])
                buf_printf(&b, "__set_range(&__st%d, (int64_t)(%s), (int64_t)(%s), %lld, %lld); ", id, gx(e->args.items[i]),
                           gx(e->args2.items[i]), t->lo, t->hi);
            else
                buf_printf(&b, "__set_incl(&__st%d, (int64_t)(%s), %lld, %lld); ", id, gx(e->args.items[i]), t->lo, t->hi);
        }
        buf_printf(&b, "__st%d; })", id);
        return xstrdup(buf_str(&b));
    }
    case E_LIST: { /* array literal */
        Buf b = {0};
        if (e->type->kind == T_DYN) { /* (1, 2, 3) for 'array of T': a fresh buffer */
            int id = ++tmpc;
            char *et = ctype(e->type->base);
            buf_printf(&b, "({ %s __l%d; __l%d.len = %d; __l%d.a = (%s*)__ppc_alloc(%d * sizeof(%s)); ", ctype(e->type), id, id,
                       e->args.len, id, et, e->args.len ? e->args.len : 1, et);
            for (int i = 0; i < e->args.len; i++) buf_printf(&b, "__l%d.a[%d] = %s; ", id, i, lit_elem(e->args.items[i], 1));
            buf_printf(&b, "__l%d; })", id);
            return b.d;
        }
        buf_printf(&b, "((%s){ { ", ctype(e->type));
        for (int i = 0; i < e->args.len; i++) buf_printf(&b, "%s%s", i ? ", " : "", lit_elem(e->args.items[i], has_owned(e->type)));
        buf_put(&b, " } })");
        return b.d;
    }
    case E_RECLIT: { /* record literal: missing fields are zero */
        Buf b = {0};
        buf_printf(&b, "((%s){ ", ctype(e->type));
        for (int i = 0; i < e->fnames.len; i++) {
            const char *fc = NULL;
            for (int k = 0; k < e->type->st->fields.len; k++)
                if (STREQ(e->type->st->fields.items[k]->name, e->fnames.items[i].name)) fc = e->type->st->fields.items[k]->cname;
            buf_printf(&b, "%s.%s = %s", i ? ", " : "", fc, lit_elem(e->args.items[i], has_owned(e->type)));
        }
        if (!e->fnames.len) buf_put(&b, "0");
        buf_put(&b, " })");
        return b.d;
    }
    case E_ISSOME: {
        Var *k = e->var;
        if (e->a->type->kind == T_RES) { /* r is ok v / r is error e */
            Type *rt = e->a->type;
            if (rt->base && (is_ref_type(rt->base) || has_owned(rt->base)) && res_temp(e->a)) {
                /* a fresh result with an object: 'is ok x' takes the object, otherwise it is freed */
                take_temp(e->a);
                int id = ++tmpc;
                char *s = sfmt("__s%d", id);
                char *pre = sfmt("%s %s = %s;", ctype(rt), s, gx(e->a));
                /* the result is owned here: the part that is not taken is freed */
                int ref = is_ref_type(rt->base);
                char *free_v = ref ? sfmt("if (!%s.err && %s.v) %s;", s, s, drop_code(rt->base, sfmt("%s.v", s)))
                                   : sfmt("if (!%s.err) %s;", s, agg_drop(rt->base, sfmt("%s.v", s)));
                char *drop_k = k ? (ref ? sfmt("%s ? (%s, 0) : 0", var_ref(k), drop_code(k->type, var_ref(k))) : sfmt("(%s, 0)", agg_drop(k->type, var_ref(k)))) : "";
                char *free_e = has_str(rt->err) ? sfmt("if (%s.err) %s;", s, agg_drop(rt->err, sfmt("%s.e", s))) : "";
                if (k && e->op == 1)
                    return sfmt("({ %s %s !%s.err ? (%s, %s = %s.v, 1) : 0; })", pre, free_e, s, drop_k, var_ref(k), s);
                if (!k) return sfmt("({ %s %s %s %s%s.err; })", pre, free_v, free_e, e->op == 1 ? "!" : "", s);
                return sfmt("({ %s %s %s.err ? (%s, %s = %s.e, 1) : 0; })", pre, free_v, s,  /* the error moves into e */
                            has_str(k->type) ? agg_drop(k->type, var_ref(k)) : "0", var_ref(k), s);
            }
            if (!k) return sfmt("(%s(%s).err)", e->op == 1 ? "!" : "", gx(e->a));
            int id = ++tmpc;
            if (has_str(k->type) && !has_owned(k->type))
                return sfmt("({ %s __s%d = %s; %s__s%d.err ? (%s, %s = __s%d.%s, %s, 1) : 0; })", ctype(e->a->type), id, gx(e->a),
                            e->op == 1 ? "!" : "", id, agg_drop(k->type, var_ref(k)), var_ref(k), id, e->op == 1 ? "v" : "e",
                            agg_copy(k->type, var_ref(k)));
            return sfmt("({ %s __s%d = %s; %s__s%d.err ? (%s = __s%d.%s, 1) : 0; })", ctype(e->a->type), id, gx(e->a),
                        e->op == 1 ? "!" : "", id, var_ref(k), id, e->op == 1 ? "v" : "e");
        }
        if (e->a->type->kind == T_OPT && has_owned(k->type) && res_temp(e->a)) {
            /* a fresh optional value that owns objects: k takes it over */
            take_temp(e->a);
            int id = ++tmpc;
            return sfmt("({ %s __s%d = %s; __s%d.has ? (%s, %s = __s%d.v, 1) : 0; })", ctype(e->a->type), id, gx(e->a), id,
                        agg_drop(k->type, var_ref(k)), var_ref(k), id);
        }
        if (e->a->type->kind == T_OPT) {
            int id = ++tmpc;
            if (has_str(k->type) && !has_owned(k->type))
                return sfmt("({ %s __s%d = %s; __s%d.has ? (%s, %s = __s%d.v, %s, 1) : 0; })", ctype(e->a->type), id, gx(e->a),
                            id, agg_drop(k->type, var_ref(k)), var_ref(k), id, agg_copy(k->type, var_ref(k)));
            return sfmt("({ %s __s%d = %s; __s%d.has ? (%s = __s%d.v, 1) : 0; })", ctype(e->a->type), id, gx(e->a), id,
                        var_ref(k), id);
        }
        if (owner_var(k)) /* owning binding (from Take/a fresh object): free what it held before */
            return sfmt("({ if (%s) %s; (%s = %s) != 0; })", var_ref(k), drop_code(k->type, var_ref(k)), var_ref(k), gx(e->a));
        return sfmt("((%s = %s) != 0)", var_ref(k), gx(e->a));
    }
    case E_CONV: {
        Type *to = e->type, *from = e->a->type;
        if (to->kind == T_RES) return sfmt("((%s){.err = 0, .v = %s})", ctype(to), gx(e->a));
        if (to->kind == T_OPT) {
            if (from->kind == T_NIL) return sfmt("((%s){0})", ctype(to));
            return sfmt("((%s){1, %s})", ctype(to), gx(e->a));
        }
        char *a = gx(e->a);
        if (to->kind == T_REAL) return sfmt("((double)(%s))", a);
        if (to->kind == T_STR && from->kind == T_CHAR) return sfmt("__pstr_char(%s)", a);
        if (to->kind == T_PTR || to->kind == T_CLASS) return sfmt("((%s)(%s))", ctype(to), a);
        if (to->kind == T_PROC && to->closure && !from->closure) return sfmt("((__Closure){(void*)(%s), 0})", a);
        return a;
    }
    case E_CAST: {
        Type *t = e->type;
        if (t->kind == T_RECORD) return sfmt("(*(%s*)&%s)", ctype(t), gx(e->a));
        return sfmt("((%s)(%s))", ctype(t), gx(e->a));
    }
    case E_BUILTIN:
        if (e->bi == BI_ARRCLONE && has_owned(e->type) && !is_taken(e))
            error_at(e->pos, "the clone owns objects and would never be freed here: store it in a variable first ('val c := a.Clone')");
        return gbuiltin(e);
    case E_METHREF: { /* obj.Show as a value: the method (the object's version) and the object */
        Routine *m = e->rt;
        Type *bt = e->a->type;
        int id = ++tmpc;
        if (bt->kind == T_RECORD)
            return sfmt("((__Closure){(void*)%s, (void*)&(%s)})", m->cname, gx(e->a));
        char *fn;
        if (e->cmode == CM_VIRTUAL) fn = sfmt("((struct VMT%d*)(__mo%d->__vmt))->v%d", bt->st->id, id, m->slot);
        else if (e->cmode == CM_IFACE) fn = sfmt("__itab((void*)__mo%d, &IID%d)[%d]", id, m->owner->id, m->slot);
        else fn = m->cname;
        return sfmt("({ %s __mo%d = %s; (__Closure){(void*)%s, (void*)__mo%d}; })", ctype(bt), id, gx(e->a), fn, id);
    }
    case E_TYPEREF: /* a class as a value: its class reference */
        if (e->type && e->type->kind == T_CLASSREF) return sfmt("((void*)&VMTI%d)", e->ctype->st->id);
        error_at(e->pos, "internal error: type used as a value");
    case E_ROUTREF: case E_LAMBDA:
        if (e->type->closure && e->rt->level > 1) /* a closure: the nested routine and the frame of the routine around it */
            return sfmt("((__Closure){(void*)%s, (void*)%s})", e->rt->cname, frame_ptr(e->rt->level - 1));
        return e->rt->cname; /* the C function itself */
    case E_RCALL:
        if (e->rt->ret_ref) { /* a reference into a container: read the value (none if there is none) */
            int id = ++tmpc;
            return sfmt("({ %s *__p%d = %s; __p%d ? *__p%d : (%s){0}; })", ctype(e->type), id, gcall(e), id, id, ctype(e->type));
        }
        if (has_owned(e->type) && !is_taken(e))
            error_at(e->pos, "the result of '%s' owns objects and would never be freed here: store it in a variable first ('val r := %s(...)')",
                     e->rt->name, e->rt->name);
        return gcall(e);
    default: error_at(e->pos, "internal error: unresolved expression (kind %d)", e->kind);
    }
}

/* ============================================================== statements */

/* stack object: point the reference at its storage in the frame and set the VMT */
/* an element of a literal: a literal that is moved (it owns objects or a buffer) needs its own strings */
static char *lit_elem(Expr *x, int own) {
    if (!own || !has_str(x->type) || ((x->kind == E_LIST || x->kind == E_RECLIT) && has_owned(x->type))) return gx(x);
    if (x->type->kind == T_STR) return sfmt("__pstr_dup(%s)", gx(x));
    int id = ++tmpc;
    return sfmt("({ %s __c%d = %s; %s; __c%d; })", ctype(x->type), id, gx(x), agg_copy(x->type, sfmt("__c%d", id)), id);
}

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
        } else if (agg_owner_var(v))
            line("%s;", agg_drop(v->type, var_ref(v)));
    }
}

/* assignment; owning places free their old value, a moved-from owner variable becomes empty */
static void gassign(Expr *lhs, Expr *rhs) {
    take_temp(rhs);
    char *l = gx(lhs), *r = gx(rhs);
    if (has_str(lhs->type) && !has_owned(lhs->type)) { /* strings: the target gets its own copy, its old value is freed */
        int id = ++tmpc;
        char *ct = ctype(lhs->type);
        line("{ %s __n%d = %s; %s; %s __o%d = %s; %s = __n%d; %s; }", ct, id, r, agg_copy(lhs->type, sfmt("__n%d", id)), ct, id, l,
             l, id, agg_drop(lhs->type, sfmt("__o%d", id)));
        return;
    }
    if (has_owned(lhs->type)) { /* a value that owns objects moves: empty the source, free the old value */
        int id = ++tmpc;
        char *ct = ctype(lhs->type);
        Expr *src = rhs;
        while ((src->kind == E_CONV || src->kind == E_OK || src->kind == E_ERR) && src->a) src = src->a; /* ok(x) moves x too */
        Buf b = {0};
        buf_printf(&b, "{ %s __n%d = %s; ", ct, id, r);
        if (cg_lvalue(src)) {
            char *sx = gx(src);
            buf_printf(&b, "memset(&(%s), 0, sizeof(%s)); ", sx, sx);
        }
        buf_printf(&b, "%s __o%d = %s; %s = __n%d; %s; }", ct, id, l, l, id, agg_drop(lhs->type, sfmt("__o%d", id)));
        line("%s", buf_str(&b));
        return;
    }
    if (!owning_place(lhs)) {
        line("%s = %s;", l, r);
        return;
    }
    if (stack_var_of(rhs)) r = box_move(stack_var_of(rhs), lhs->type);
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
    for (int i = 0; i < v->len; i++) {
        gs(v->items[i]);
        line("__ppc_release(__sm);"); /* temporary strings of the statement */
    }
}

/* Break/Continue leave the blocks inside the loop: their owners free their objects */
static void leave_blocks(void) {
    if (!loop_blk.len) return;
    for (int k = cg_blocks.len - 1; k >= loop_blk.items[loop_blk.len - 1]; k--)
        stack_destroy(cg_blocks.items[k]->wvars.items, cg_blocks.items[k]->wvars.len);
}

static void gs_inner(Stmt *s);
/* every C line of a statement gets the statement's place in the Pascal source (#line): gcc messages and gdb
   refer to the .pas file; nested statements have their own place, the outer one is restored after them */
static void gs(Stmt *s) {
    Pos save = line_pos;
    int had = have_line_pos;
    if (s->kind != S_COMPOUND && s->kind != S_BLOCK && s->kind != S_EMPTY && s->pos.line > 0) {
        line_pos = s->pos;
        have_line_pos = 1;
    }
    gs_inner(s);
    line_pos = save;
    have_line_pos = had;
}
static void gs_inner(Stmt *s) {
    switch (s->kind) {
    case S_EMPTY: case S_FORIN: return; /* for-in loops are rewritten by sema */
    case S_ASSIGN:
        if (s->bind == 3) { /* val k := h: the heap object moves into the frame, its memory is given back */
            Var *k = s->a->var, *mv = move_source(s->b);
            int id = ++tmpc, sid = k->type->st->id;
            line("{ struct S%d *__h%d = (struct S%d*)%s;", sid, id, sid, nilchk(s->b, gx(s->b), s->pos));
            line("  if (*(void**)__h%d != (void*)&VMTI%d) { fflush(stdout); fprintf(stderr, \"Runtime error 219: a %%s object does not fit into stack object '%s' (%s) at %%s(%d,%d)\\n\", (*(struct __VMTHdr**)__h%d)->name, __ppc_srcfile); exit(219); }",
                 id, sid, k->name, k->type->st->name, s->pos.line, s->pos.col, id);
            line("  memcpy(%s, __h%d, sizeof(struct S%d)); __ppc_free(__h%d); %s = 1;%s }", var_raw(k), id, sid, id,
                 var_aux(k, "__live"), mv ? sfmt(" %s = 0;", var_ref(mv)) : "");
            return;
        }
        if (s->bind == 2) { /* val b := k: the stack object moves (byte copy), k is dead afterwards */
            Var *b = s->a->var, *k = s->b->var;
            line("memcpy(%s, %s, sizeof(struct S%d)); %s = 1; %s = 0;", var_raw(b), var_raw(k), b->type->st->id,
                 var_aux(b, "__live"), var_aux(k, "__live"));
            return;
        }
        if (s->bind == 4) { /* Result := none in a function with a 'var' result */
            line("%s = 0;", var_raw(s->a->var));
            return;
        }
        if (s->bind) { /* var x := y: x refers to y */
            line("%s = &%s;", var_raw(s->a->var), gx(s->b));
            return;
        }
        if (s->a->kind == E_DEREF && s->a->a->kind == E_VAR && s->a->a->var->is_box) {
            /* storing into a heap value allocates its cell on first use */
            Var *v = s->a->a->var;
            char *x = var_ref(v), *ct = ctype(v->type->base);
            line("if (!%s) %s = (%s*)__ppc_alloc(sizeof(%s));", x, x, ct, ct);
            if (needs_drop(v->type->base)) gassign(s->a, s->b);
            else line("*%s = %s;", x, gx(s->b));
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
        vpush(cg_blocks, s);
        gstmts(&s->blk->body);
        cg_blocks.len--;
        stack_destroy(s->wvars.items, s->wvars.len);
        indent--;
        line("}");
        return;
    case S_CALL:
        if (s->a->kind == E_RCALL && has_owned(s->a->type) && !s->a->rt->ret_ref) { /* unused value that owns objects */
            take_temp(s->a);
            int id = ++tmpc;
            line("{ %s __d%d = %s; %s; }", ctype(s->a->type), id, gx(s->a), agg_drop(s->a->type, sfmt("__d%d", id)));
            return;
        }
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
        {
            VarList ow = {0};
            cond_owners(s->a, &ow);
            for (int i = 0; i < ow.len; i++)
                if (is_ref_type(ow.items[i]->type))
                    line("if (%s) { %s; %s = 0; }", var_ref(ow.items[i]), drop_code(ow.items[i]->type, var_ref(ow.items[i])), var_ref(ow.items[i]));
                else
                    line("%s;", agg_drop(ow.items[i]->type, var_ref(ow.items[i])));
        }
        return;
    case S_WHILE:
        vpush(loops, 0);
        vpush(loop_blk, cg_blocks.len);
        if (s->a) line("while (%s) {", gx(s->a));
        else line("for (;;) {");
        indent++;
        gs(s->s1);
        line("__ppc_release(__sm);");
        indent--;
        line("}");
        loops.len--;
        loop_blk.len--;
        return;
    case S_REPEAT:
        vpush(loops, 0);
        vpush(loop_blk, cg_blocks.len);
        line("do {");
        indent++;
        gstmts(&s->body);
        indent--;
        line("} while (!(%s));", gx(s->a));
        loops.len--;
        loop_blk.len--;
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
        vpush(loop_blk, cg_blocks.len);
        gs(s->s1);
        line("__ppc_release(__sm);");
        loops.len--;
        loop_blk.len--;
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
    case S_BREAK:
        leave_blocks();
        line("break;");
        return;
    case S_CONTINUE:
        leave_blocks();
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
    if (r->kind == RK_FUNC && r->ret_ref) return sfmt("%s*", ctype(r->ret));
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
    if (r->owner && !r->is_classm) buf_printf(&b, "%sstruct S%d *%s", SEP(), r->owner->id, names ? "self" : "");
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

/* ---- libraries: the exported routines under their C names ---- */
static const char *C_KEYWORDS[] = {"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum",
                                   "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return",
                                   "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void",
                                   "volatile", "while", "bool", "true", "false", NULL};
static const char *c_param_name(const char *n) {
    for (int i = 0; C_KEYWORDS[i]; i++)
        if (STREQ(C_KEYWORDS[i], n)) return sfmt("%s_", n);
    return n;
}
/* wrapper: 'int32_t Add(int32_t a, int32_t b)' calls the routine; String parameters come as C strings */
static void gen_exports(Program *p) {
    for (int i = 0; i < p->exports.len; i++) {
        Routine *r = p->exports.items[i].rt;
        Buf ps = {0}, pre = {0}, args = {0};
        for (int k = 0; k < r->params.len; k++) {
            Param *pm = &r->params.items[k];
            const char *pn = sfmt("p%d", k);
            if (k) buf_put(&ps, ", ");
            if (k) buf_put(&args, ", ");
            if (pm->type->kind == T_STR) {
                buf_printf(&ps, "const char *%s", pn);
                buf_printf(&pre, "  PStr s%d = %s ? __pstr_cstr(%s) : __pstr_lit(\"\", 0);\n", k, pn, pn);
                buf_printf(&args, "s%d", k);
            } else {
                int ref = pm->mode == PM_CONSTREF;
                buf_printf(&ps, "%s%s %s", ctype(pm->type), ref ? "*" : "", pn);
                buf_put(&args, pn);
            }
        }
        if (!r->params.len) buf_put(&ps, "void");
        const char *rt = r->ret ? ctype(r->ret) : "void";
        buf_printf(&b_funcs, "__attribute__((visibility(\"default\"))) %s %s(%s) {\n", rt, p->exports.items[i].cname, buf_str(&ps));
        buf_put(&b_funcs, "  size_t __sm = __ppc_mark();\n");
        buf_put(&b_funcs, buf_str(&pre));
        if (r->ret) buf_printf(&b_funcs, "  %s __r = %s(%s);\n", rt, r->cname, buf_str(&args));
        else buf_printf(&b_funcs, "  %s(%s);\n", r->cname, buf_str(&args));
        buf_put(&b_funcs, "  __ppc_release(__sm);\n  fflush(stdout);\n");
        if (r->ret) buf_put(&b_funcs, "  return __r;\n");
        buf_put(&b_funcs, "}\n");
    }
}

/* the C header of a library: its types and exported routines */
static VEC(Type *) hdr_done;
static const char *hdr_type(Type *t, Buf *types) {
    switch (t->kind) {
    case T_BOOL: return "bool";
    case T_CHAR: return "char";
    case T_REAL: return "double";
    case T_STR: return "const char *";
    case T_ENUM: case T_RECORD: {
        const char *n = t->kind == T_ENUM ? t->en->name : t->st->name;
        for (int i = 0; i < hdr_done.len; i++)
            if (hdr_done.items[i] == t || (t->kind == T_RECORD && hdr_done.items[i]->kind == T_RECORD && hdr_done.items[i]->st == t->st) ||
                (t->kind == T_ENUM && hdr_done.items[i]->kind == T_ENUM && hdr_done.items[i]->en == t->en))
                return n;
        vpush(hdr_done, t);
        if (t->kind == T_ENUM) {
            buf_printf(types, "typedef int32_t %s;\nenum {", n);
            for (int i = 0; i < t->en->names.len; i++) buf_printf(types, "%s %s_%s = %d", i ? "," : "", n, t->en->names.items[i], i);
            buf_put(types, " };\n\n");
        } else {
            Buf fs = {0};
            for (int i = 0; i < t->st->fields.len; i++) {
                Field *f = t->st->fields.items[i];
                buf_printf(&fs, "    %s %s;\n", hdr_type(f->type, types), c_param_name(f->name));
            }
            buf_printf(types, "typedef struct %s {\n%s} %s;\n\n", n, buf_str(&fs), n);
        }
        return n;
    }
    default: return ctype(t);
    }
}
void codegen_header(Program *p, FILE *f) {
    Buf types = {0}, fns = {0};
    hdr_done.len = 0;
    for (int i = 0; i < p->exports.len; i++) {
        Routine *r = p->exports.items[i].rt;
        buf_printf(&fns, "%s %s(", r->ret ? hdr_type(r->ret, &types) : "void", p->exports.items[i].cname);
        for (int k = 0; k < r->params.len; k++) {
            Param *pm = &r->params.items[k];
            const char *t = hdr_type(pm->type, &types);
            buf_printf(&fns, "%s%s%s%s", k ? ", " : "", pm->mode == PM_CONSTREF ? "const " : "", t, pm->mode == PM_CONSTREF ? " *" : (pm->type->kind == T_STR ? "" : " "));
            buf_put(&fns, c_param_name(pm->name ? pm->name : sfmt("p%d", k)));
        }
        if (!r->params.len) buf_put(&fns, "void");
        buf_put(&fns, ");\n");
    }
    char *guard = sfmt("%s_H", p->name);
    for (char *c = guard; *c; c++) *c = (char)toupper((unsigned char)*c);
    fprintf(f, "/* generated by ppc - Perfect Pascal Compiler; library %s */\n", p->name);
    fprintf(f, "#ifndef %s\n#define %s\n\n#include <stdint.h>\n#include <stdbool.h>\n\n", guard, guard);
    fprintf(f, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n%s%s\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n", buf_str(&types), buf_str(&fns));
}

/* function-pointer type of a virtual slot: self is void* */
static char *slot_type(Routine *r, const char *name) {
    Buf b = {0};
    buf_printf(&b, "%s (*%s)(void*", ret_ctype(r), name ? name : "");
    if (r->kind == RK_DTOR) buf_put(&b, ", int");
    if (r->kind == RK_CTOR) buf_put(&b, ", void*"); /* constructors: the VMT of the object to make */
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
    loop_blk.len = 0;
    cg_blocks.len = 0;
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
            buf_printf(&f, "  %s%s %s;\n", ctype(v->type), v->is_ref ? "*" : "", v->cname);
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
        if (r->result && r->ret_ref) line("%s *Result = 0;", ctype(r->result->type));
        else if (r->result) line("%s Result; memset(&Result, 0, sizeof Result);", ctype(r->result->type));
        for (int i = 0; i < r->locals.len; i++) {
            Var *v = r->locals.items[i];
            line("%s%s %s; memset(&%s, 0, sizeof %s);", ctype(v->type), v->is_ref ? "*" : "", v->cname, v->cname, v->cname);
            if (v->is_stack)
                line("struct S%d %s__st; memset(&%s__st, 0, sizeof %s__st); int %s__live = 0;", v->type->st->id, v->cname,
                     v->cname, v->cname, v->cname);
        }
    }
    line("size_t __sm = __ppc_mark();");
    for (int i = 0; i < r->params.len; i++) { /* value parameters with strings get their own copy */
        Var *v = r->params.items[i].var;
        if (r->params.items[i].mode == PM_VALUE && has_str(v->type) && !has_owned(v->type)) line("%s;", agg_copy(v->type, var_ref(v)));
    }
    for (int i = 0; i < r->locals.len; i++) stack_init(r->locals.items[i]);
    gstmts(&r->body);
    line_reset(); /* the clean-up code belongs to the C file */
    line("goto __exit;");
    indent = 0;
    line("__exit:;");
    indent = 1;
    stack_destroy(r->locals.items, r->locals.len);
    for (int i = r->params.len - 1; i >= 0; i--)
        if (r->params.items[i].mode == PM_OWNED) stack_destroy(&r->params.items[i].var, 1);
    for (int i = 0; i < r->params.len; i++) {
        Var *v = r->params.items[i].var;
        if (r->params.items[i].mode == PM_VALUE && has_str(v->type) && !has_owned(v->type)) line("%s;", agg_drop(v->type, var_ref(v)));
    }
    line("__ppc_release(__sm);");
    if (r->result && has_str(r->result->type) && !has_owned(r->result->type))
        line("%s;", agg_tmp(r->result->type, var_ref(r->result))); /* the caller's statement frees the result */
    if (r->kind == RK_DTOR && r->owner->kind == SK_CLASS)
        line("if (__free) { ((struct __VMTHdr*)self->__vmt)->drop(self); __ppc_free(self); }");
    if (r->result) line("return %s;", r->ret_ref ? var_raw(r->result) : var_ref(r->result));
    else if (r->kind == RK_CTOR && r->owner->kind == SK_CLASS) line("return self;");
    indent = 0;
    line("}");
    line("");
    buf_put(&b_funcs, buf_str(&body));
}
/* struct tags of classes and records: the type's name and its number, e.g. 'struct TCircle12'
   (generic instances: 'struct TList_TCustomer_7'). The code is generated with 'struct S12' and renamed on output. */
static char **tag_names;
static int tag_count;
static void make_tag_names(Program *p) {
    tag_count = 0;
    for (int i = 0; i < p->structs.len; i++)
        if (p->structs.items[i]->id >= tag_count) tag_count = p->structs.items[i]->id + 1;
    tag_names = xcalloc((size_t)tag_count + 1, sizeof(char *));
    static const char *taken[] = {"S", "D", "R", "O", "F", "A", "VMT", NULL}; /* other generated tags */
    for (int i = 0; i < p->structs.len; i++) {
        Struct *st = p->structs.items[i];
        Buf b = {0};
        for (const char *c = st->name; *c; c++) { /* letters, digits, '_'; everything else becomes one '_' */
            int ok = isalnum((unsigned char)*c) || *c == '_';
            if (ok) buf_putn(&b, c, 1);
            else if (b.len && b.d[b.len - 1] != '_') buf_put(&b, "_");
        }
        while (b.len && b.d[b.len - 1] == '_') b.d[--b.len] = 0;
        const char *base = b.len ? buf_str(&b) : "T";
        int sep = isdigit((unsigned char)base[strlen(base) - 1]); /* the number must stay recognizable: TVec3_12 */
        for (int k = 0; taken[k]; k++) sep |= STREQ(base, taken[k]);
        tag_names[st->id] = sfmt("%s%s%d", base, sep ? "_" : "", st->id);
    }
}
static int out_lines; /* lines of the C file written so far */
static void put_plain(const char *s, size_t n, FILE *f) {
    const char *rs = LINE_RESET;
    size_t rl = strlen(rs);
    while (n) {
        const char *h = NULL;
        for (const char *q = s; q + rl <= s + n; q++)
            if (*q == '#' && strncmp(q, rs, rl) == 0) { h = q; break; }
        size_t k = h ? (size_t)(h - s) : n;
        for (size_t i = 0; i < k; i++) out_lines += s[i] == '\n';
        fwrite(s, 1, k, f);
        if (!h) break;
        fprintf(f, "#line %d \"%s\"", out_lines + 2, g_cfile); /* the next line is line out_lines + 2 */
        s = h + rl;
        n -= k + rl;
    }
}
static void put_named(const char *s, FILE *f) {
    const char *pat = "struct S";
    size_t n = strlen(pat);
    for (;;) {
        const char *h = strstr(s, pat);
        if (!h) break;
        const char *d = h + n;
        int id = 0, nd = 0;
        while (isdigit((unsigned char)d[nd])) id = id * 10 + (d[nd++] - '0');
        int word = nd && !isalnum((unsigned char)d[nd]) && d[nd] != '_' && (h == s || (!isalnum((unsigned char)h[-1]) && h[-1] != '_'));
        if (word && id < tag_count && tag_names[id]) {
            put_plain(s, (size_t)(h - s), f);
            fprintf(f, "struct %s", tag_names[id]);
            s = d + nd;
        } else {
            put_plain(s, (size_t)(d - s), f);
            s = d;
        }
    }
    put_plain(s, strlen(s), f);
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
    for (int i = 0; i < p->structs.len; i++)
        if (p->structs.items[i]->is_iface) buf_printf(&b_globals, "static const char IID%d = 0; /* %s */\n", p->structs.items[i]->id, p->structs.items[i]->name);
    for (int i = 0; i < p->structs.len; i++) {
        Struct *s = p->structs.items[i];
        if (!s->has_vmt) continue;
        /* interface tables: for every interface of the class and its ancestors (and what they extend) */
        VEC(Struct *) ifs = {0};
        for (Struct *c = s; c; c = c->parent)
            for (int k = 0; k < c->ifaces.len; k++)
                for (Struct *it = c->ifaces.items[k]; it; it = it->parent) {
                    int dup = 0;
                    for (int j = 0; j < ifs.len; j++) dup |= ifs.items[j] == it;
                    if (!dup) vpush(ifs, it);
                }
        for (int k = 0; k < ifs.len; k++) {
            Struct *it = ifs.items[k];
            buf_printf(&b_vmti, "static void *ITF%d_%d[] = {", s->id, it->id);
            for (int j = 0; j < it->methods.len; j++) {
                Routine *im = it->methods.items[j], *m = NULL;
                for (Struct *c = s; c && !m; c = c->parent)
                    for (int q = 0; q < c->methods.len && !m; q++) {
                        Routine *cm = c->methods.items[q];
                        if (!STREQ(cm->name, im->name) || cm->params.len != im->params.len) continue;
                        int same = 1; /* overloads: the variant with the interface's parameters */
                        for (int a = 0; a < cm->params.len; a++)
                            same &= STREQ(ctype(cm->params.items[a].type), ctype(im->params.items[a].type));
                        if (same) m = cm;
                    }
                if (m && m->slot >= 0) m = s->slots.items[m->slot].impl; /* virtual: this class's version */
                buf_printf(&b_vmti, " (void*)%s,", !m || m->is_abstract ? "__abstract_err" : m->cname);
            }
            buf_put(&b_vmti, " 0 };\n");
        }
        if (ifs.len) {
            buf_printf(&b_vmti, "static const struct __ITab ITABS%d[] = {", s->id);
            for (int k = 0; k < ifs.len; k++) buf_printf(&b_vmti, " { &IID%d, ITF%d_%d },", ifs.items[k]->id, s->id, ifs.items[k]->id);
            buf_put(&b_vmti, " { 0, 0 } };\n");
        }
        buf_printf(&b_vmtt, "struct VMT%d { /* %s */\n  struct __VMTHdr h;\n", s->id, s->name);
        for (int k = 0; k < s->slots.len; k++) buf_printf(&b_vmtt, "  %s;\n", slot_type(s->slots.items[k].intro, sfmt("v%d", k)));
        buf_put(&b_vmtt, "};\n");
        buf_printf(&b_globals, "extern struct VMT%d VMTI%d;\n", s->id, s->id);
        int cl = needs_clone(s);
        if (cl) gen_clonefix(s);
        buf_printf(&b_vmti, "struct VMT%d VMTI%d = {\n  { %s, \"%s\", sizeof(struct S%d), D%d, %s, %s },\n", s->id, s->id,
                   s->parent && s->parent->has_vmt ? sfmt("&VMTI%d", s->parent->id) : "NULL", s->name, s->id, s->id,
                   cl ? sfmt("K%d", s->id) : "0", ifs.len ? sfmt("ITABS%d", s->id) : "0");
        /* drop function: frees the fields that own an object (runs after Destroy) */
        buf_printf(&b_protos, "static void D%d(void *o);\n", s->id);
        Buf db = {0}; /* nested drop functions are emitted first */
        buf_printf(&db, "static void D%d(void *o) {\n  struct S%d *s = (struct S%d*)o; (void)s;\n", s->id, s->id, s->id);
        for (int k = 0; k < s->fields.len; k++) {
            Field *f = s->fields.items[k];
            char *x = sfmt("s->%s", f->cname);
            if (needs_drop(f->type)) buf_printf(&db, "  %s;\n", agg_drop(f->type, x));
            if (!is_ref_type(f->type) || f->borrowed) continue;
            buf_printf(&db, "  if (%s) { %s; %s = 0; }\n", x, drop_code(f->type, x), x);
        }
        buf_put(&db, "}\n");
        buf_put(&b_drops, buf_str(&db));
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
        buf_printf(&b_globals, "static %s%s %s;\n", ctype(v->type), v->is_ref ? "*" : "", v->cname);
        if (v->is_stack) buf_printf(&b_globals, "static struct S%d %s__st;\nstatic int %s__live;\n", v->type->st->id, v->cname, v->cname);
    }
    /* prototypes and bodies */
    for (int i = 0; i < p->routines.len; i++) {
        Routine *r = p->routines.items[i];
        if (r->is_abstract || r->is_auto || r->is_sig || r->is_external) continue;
        buf_printf(&b_protos, "static %s;\n", sig(r, 0));
    }
    for (int i = 0; i < p->routines.len; i++) {
        Routine *r = p->routines.items[i];
        if (r->is_abstract || r->is_auto || r->is_sig || r->is_external) continue;
        buf_put(&b_funcs, "static ");
        gen_routine(r);
    }
    for (int i = 0; i < p->structs.len; i++)
        for (int k = 0; k < p->structs.items[i]->methods.len; k++)
            if (p->structs.items[i]->methods.items[k]->is_auto) gen_auto_clone(p->structs.items[i]->methods.items[k]);
    /* main */
    Buf mb = {0};
    out = &mb;
    cur = NULL;
    level = 0;
    loops.len = 0;
    loop_blk.len = 0;
    cg_blocks.len = 0;
    indent = 0;
    if (p->is_library) { /* a shared library: its main block runs when it is loaded, its globals are freed when it is unloaded */
        line("__attribute__((constructor)) static void __ppc_lib_init(void) {");
        indent = 1;
        line("size_t __sm = __ppc_mark();");
    } else {
        line("int main(int argc, char **argv) {");
        indent = 1;
        line("__ppc_argc = argc; __ppc_argv = argv;");
        line("size_t __sm = __ppc_mark();");
    }
    for (int i = 0; i < p->globals.len; i++) stack_init(p->globals.items[i]);
    gstmts(&p->init);
    gstmts(&p->body);
    line_reset();
    line("goto __exit;");
    indent = 0;
    line("__exit:;");
    indent = 1;
    if (p->is_library) {
        line("__ppc_release(__sm);");
        line("fflush(stdout);");
        indent = 0;
        line("}");
        line("__attribute__((destructor)) static void __ppc_lib_fini(void) {");
        indent = 1;
    }
    stack_destroy(p->globals.items, p->globals.len);
    line("fflush(stdout);");
    if (p->is_library) line("free(__tp); __tp = NULL; __tpn = __tpc = 0; /* the pool of temporary strings (dlclose) */");
    if (!p->is_library) line("return 0;");
    indent = 0;
    line("}");
    if (p->is_library) gen_exports(p);

    fprintf(f, "/* generated by ppc - Perfect Pascal Compiler; program %s */\n", p->name);
    make_tag_names(p);
    out_lines = 1; /* the comment line above */
    put_plain(PPC_RUNTIME, strlen(PPC_RUNTIME), f);
    put_named("\n/* ---- forward declarations ---- */\n", f);
    put_named(buf_str(&b_fwd), f);
    put_named("\n/* ---- types ---- */\n", f);
    put_named(buf_str(&b_types), f);
    put_named("\n/* ---- virtual method tables ---- */\n", f);
    put_named(buf_str(&b_vmtt), f);
    put_named("\n/* ---- globals ---- */\n", f);
    put_named(buf_str(&b_globals), f);
    put_named("\n/* ---- prototypes ---- */\n", f);
    put_named(buf_str(&b_protos), f);
    put_named("\n", f);
    put_named(buf_str(&b_vmti), f);
    put_named("\n/* ---- frames for nested routines ---- */\n", f);
    put_named(buf_str(&b_frames), f);
    put_named("\n/* ---- routines ---- */\n", f);
    put_named(buf_str(&b_drops), f);
    put_named(buf_str(&b_funcs), f);
    put_named(buf_str(&mb), f);
}
