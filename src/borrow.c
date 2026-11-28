/* borrow.c - ownership & borrow checker.
 *
 * Model (inspired by Rust, adapted to Object Pascal):
 *   - Variables, fields and function results of class or typed-pointer type
 *     OWN the object they refer to, unless declared with 'var'.
 *   - Parameters of such types are borrows ('T const': read-only); 'vao'
 *     parameters take ownership; var/out parameters are mutable borrows
 *     of the caller's owning slot.
 *   - Assigning an owner to another owner MOVES the value; the source
 *     becomes unusable until it is reassigned.
 *   - Free/Destroy/Dispose/FreeAndNil consume the owner.
 *   - A borrowed variable remembers which places it was borrowed from
 *     ("origins"). Freeing, moving or reassigning such a place makes the
 *     borrow dangling; using a dangling borrow is an error.
 *   - References to local variables and local owners must not escape into globals,
 *     fields or function results.
 *   - The same place must not be passed twice to a call if one of the
 *     parameters is var/out (no aliasing of mutable borrows).
 *
 * The analysis is flow-sensitive and intraprocedural; loops are iterated
 * to a fixed point. Effects of calls on non-local variables are
 * approximated by a write-set per routine (transitively closed). */
#include "ast.h"

enum { O_NULL = 1, O_OWNED = 2, O_MOVED = 4, O_FREED = 8, O_UNK = 16 };
/* optional values of a type parameter (optional T in generic bodies) may be empty too */
#define NILABLE(t) ((t)->nilable || (t)->kind == T_OPT)
enum { B_VALID = 1, B_DANGLING = 2, B_CONFLICT = 4, B_CARRY = 8 }; /* B_CONFLICT: aliasing rule broken while borrowed;
   B_CARRY: an owner whose object refers (through 'var' fields) to the origins in org */
enum { TV_OWNER, TV_BORROW };
enum { N_UNINIT = 1, N_NIL = 2, N_NN = 4 }; /* nil analysis: never allocated / nil / non-nil */

int g_borrow_check = 1, g_nil_check = 1, g_range_check = 1;

typedef struct {
    char *path;
    char *desc;
    Var *root;
    int stack;
    char *slot; /* carried by an owner: the 'var' field that holds the reference */
} Origin;
typedef VEC(Origin) Origins;

typedef struct {
    int reach;
    int n;
    unsigned char *own;
    unsigned char *bor;
    unsigned char *nn;
    unsigned char *fd; /* constructor analysis: own non-nilable fields + 'inherited called' flag */
    Origins *org;
    char **why;
} State;

typedef struct {
    Var *v;
    int kind;
    int varparam;
} TVar;

typedef struct {
    State brk, cont;
    int blk; /* blocks open when the loop started: Break/Continue leave the ones above */
} LoopCtx;
static VEC(Stmt *) cblocks; /* block statements being analyzed, innermost last */

static Program *prog;
static VEC(TVar) tv;
static VEC(LoopCtx *) loops;
static VEC(Field *) cfields; /* constructor being checked: own non-nilable reference fields */
static int cneed_inh;        /* constructor must call an inherited constructor */
#define NFD (cfields.len + 1)
#define FD_INH (cfields.len)
static Routine *unit;     /* NULL = main program */
static const char *unit_name;
static int nerrors;
static VEC(char *) reported;
static Routine *tobject_free;

/* per-routine write sets (non-local variables written) */
typedef VEC(Var *) VarSet;
static VarSet *wsets;
static VarSet *rsets; /* non-local non-nilable references read by each routine */

/* ============================================================== diagnostics */

static int seen(Pos p, const char *kind, const char *msg) {
    char *key = sfmt("%d:%d:%s:%s", p.line, p.col, kind, msg);
    for (int i = 0; i < reported.len; i++)
        if (STREQ(reported.items[i], key)) return 1;
    vpush(reported, key);
    return 0;
}

static const char *arch_note = ""; /* checking a generic body: its type parameters may be objects */
static void berror(Pos p, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char msg[1024];
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (!g_borrow_check || seen(p, "E", msg)) return;
    fprintf(stderr, "%s(%d,%d) Borrow error: %s%s\n", POS_FILE(p), p.line, p.col, msg, arch_note);
    nerrors++;
}

static void nerror(Pos p, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char msg[1024];
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (!g_nil_check || seen(p, "N", msg)) return;
    fprintf(stderr, "%s(%d,%d) Nil error: %s\n", POS_FILE(p), p.line, p.col, msg);
    nerrors++;
}

__attribute__((unused)) static void bwarn(Pos p, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char msg[1024];
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (!g_borrow_check || seen(p, "W", msg)) return;
    fprintf(stderr, "%s(%d,%d) Borrow warning: %s\n", POS_FILE(p), p.line, p.col, msg);
    g_warnings++;
}

/* ============================================================== state */

static State st_new(void) {
    State s = {0};
    s.reach = 1;
    s.n = tv.len;
    s.own = xcalloc((size_t)s.n + 1, 1);
    s.bor = xcalloc((size_t)s.n + 1, 1);
    s.nn = xcalloc((size_t)s.n + 1, 1);
    s.fd = xcalloc((size_t)NFD, 1);
    s.org = xcalloc((size_t)s.n + 1, sizeof(Origins));
    s.why = xcalloc((size_t)s.n + 1, sizeof(char *));
    return s;
}

static State st_copy(State *a) {
    State s = st_new();
    s.reach = a->reach;
    memcpy(s.own, a->own, (size_t)a->n);
    memcpy(s.bor, a->bor, (size_t)a->n);
    memcpy(s.nn, a->nn, (size_t)a->n);
    memcpy(s.fd, a->fd, (size_t)NFD);
    memcpy(s.why, a->why, (size_t)a->n * sizeof(char *));
    for (int i = 0; i < a->n; i++)
        for (int k = 0; k < a->org[i].len; k++) vpush(s.org[i], a->org[i].items[k]);
    return s;
}

static State st_unreach(void) {
    State s = st_new();
    s.reach = 0;
    return s;
}

static int org_has(Origins *o, const char *path) {
    for (int k = 0; k < o->len; k++)
        if (STREQ(o->items[k].path, path)) return 1;
    return 0;
}

/* a |= b */
static void st_join(State *a, State *b) {
    if (!b->reach) return;
    if (!a->reach) {
        State c = st_copy(b);
        *a = c;
        return;
    }
    for (int k = 0; k < NFD; k++) a->fd[k] |= b->fd[k];
    for (int i = 0; i < a->n; i++) {
        a->own[i] |= b->own[i];
        a->bor[i] |= b->bor[i];
        a->nn[i] |= b->nn[i];
        if (!a->why[i]) a->why[i] = b->why[i];
        for (int k = 0; k < b->org[i].len; k++)
            if (!org_has(&a->org[i], b->org[i].items[k].path)) vpush(a->org[i], b->org[i].items[k]);
    }
}

static int st_eq(State *a, State *b) {
    if (a->reach != b->reach) return 0;
    if (!a->reach) return 1;
    if (memcmp(a->fd, b->fd, (size_t)NFD)) return 0;
    for (int i = 0; i < a->n; i++) {
        if (a->own[i] != b->own[i] || a->bor[i] != b->bor[i] || a->nn[i] != b->nn[i]) return 0;
        if (a->org[i].len != b->org[i].len) return 0;
    }
    return 1;
}

/* ============================================================== paths */


static char *path_of(Expr *e, Var **root) {
    switch (e->kind) {
    case E_VAR: {
        if (root) *root = e->var;
        return sfmt("#%d", e->var->id);
    }
    case E_FIELD: {
        char *p = path_of(e->a, root);
        return p ? sfmt("%s.%s", p, e->field->name) : NULL;
    }
    case E_INDEX: {
        char *p = path_of(e->a, root);
        return p ? sfmt("%s[]", p) : NULL;
    }
    case E_DEREF: {
        char *p = path_of(e->a, root);
        return p ? sfmt("%s^", p) : NULL;
    }
    case E_CONV: case E_CAST: case E_AS: return path_of(e->a, root);
    default: return NULL;
    }
}

static char *desc_of(Expr *e) {
    switch (e->kind) {
    case E_VAR:
        return e->var->name;
    case E_FIELD:
        if (e->a->kind == E_VAR && e->a->var->vkind == VK_SELF) return e->field->name;
        return sfmt("%s.%s", desc_of(e->a), e->field->name);
    case E_INDEX: return sfmt("%s[...]", desc_of(e->a));
    case E_DEREF: return sfmt("%s^", desc_of(e->a));
    case E_CONV: case E_CAST: case E_AS: return desc_of(e->a);
    case E_RCALL: return sfmt("%s(...)", e->rt->name);
    case E_NIL: return "nil";
    default: return "expression";
    }
}

static int has_prefix(const char *p, const char *pre) {
    size_t n = strlen(pre);
    if (strncmp(p, pre, n) != 0) return 0;
    return p[n] == 0 || p[n] == '.' || p[n] == '[' || p[n] == '^';
}

static int paths_overlap(const char *a, const char *b) { return has_prefix(a, b) || has_prefix(b, a); }

/* ============================================================== tracked vars */

static int slot(Var *v) { return v ? v->bslot - 1 : -1; }

static int tracked_owner(Var *v) {
    int i = slot(v);
    return i >= 0 && tv.items[i].kind == TV_OWNER;
}
static int tracked_borrow(Var *v) {
    int i = slot(v);
    return i >= 0 && tv.items[i].kind == TV_BORROW;
}

/* owning bindings made by the condition of an 'if' ('if Open(n) is ok f'): freed when the 'if' ends */
static void cond_owners(Expr *c, VarList *out) {
    if (!c) return;
    if (c->kind == E_BINARY && c->op == OP_AND) {
        cond_owners(c->a, out);
        cond_owners(c->b, out);
    } else if (c->kind == E_ISSOME && c->var && is_ref_type(c->var->type) && !c->var->borrowed && !c->var->is_stack)
        vpush(*out, c->var);
}
static int carrier(State *s, int i) { return tv.items[i].kind == TV_OWNER && (s->bor[i] & B_CARRY); }
static void invalidate(State *s, const char *path, const char *reason) {
    for (int i = 0; i < tv.len; i++) {
        int c = carrier(s, i);
        if (tv.items[i].kind != TV_BORROW && !c) continue;
        for (int k = 0; k < s->org[i].len; k++)
            if (has_prefix(s->org[i].items[k].path, path)) {
                s->bor[i] = c ? (B_CARRY | B_DANGLING) : B_DANGLING;
                s->why[i] = (char *)reason;
                break;
            }
    }
}

static void invalidate_root(State *s, Var *root, const char *reason) {
    for (int i = 0; i < tv.len; i++) {
        if (tv.items[i].kind != TV_BORROW && !carrier(s, i)) continue;
        for (int k = 0; k < s->org[i].len; k++)
            if (s->org[i].items[k].root == root) {
                s->bor[i] |= B_DANGLING;
                if (!s->why[i]) s->why[i] = (char *)reason;
                break;
            }
    }
}
static void carry_reset(State *s, int i) {
    s->bor[i] = 0;
    s->org[i] = (Origins){0};
    s->why[i] = NULL;
}

static int is_case_sel(Var *v) { return strncmp(v->name, "__sum", 5) == 0 || strncmp(v->name, "__case", 6) == 0; }

static void check_var_use(Var *v, Pos p, State *s) {
    if (!s->reach) return;
    for (int k = 0; k < v->meth_objs.len; k++) { /* f := c.Tick: using f uses c */
        Var *o = v->meth_objs.items[k];
        int j = slot(o);
        if (j < 0) continue;
        if (tv.items[j].kind == TV_OWNER && (s->own[j] & (O_FREED | O_MOVED)))
            berror(p, "'%s' holds a method value of '%s', which %s %s", v->name, o->name, (s->own[j] & (O_OWNED | O_UNK)) ? "may have been" : "was",
                   (s->own[j] & O_FREED) ? "freed" : "moved away");
        else if (tv.items[j].kind == TV_BORROW && (s->bor[j] & B_DANGLING))
            berror(p, "'%s' holds a method value of '%s', but %s", v->name, o->name, s->why[j] ? s->why[j] : "its object is gone");
    }
    int i = slot(v);
    if (i < 0) return;
    if (tv.items[i].kind == TV_OWNER) {
        unsigned st = s->own[i];
        int maybe = (st & (O_NULL | O_OWNED | O_UNK)) != 0;
        if (st & O_MOVED)
            berror(p, "use of %smoved value '%s' (ownership was transferred away)", maybe ? "possibly " : "", v->name);
        if (st & O_FREED)
            berror(p, "use of '%s' after it %s freed", v->name, maybe ? "may have been" : "was");
        if ((s->bor[i] & B_CARRY) && (s->bor[i] & B_DANGLING))
            berror(p, "'%s' is used after the object it refers to through a 'var' field %s%s", v->name,
                   (s->bor[i] & B_VALID) ? "may be gone: " : "is gone: ", s->why[i] ? s->why[i] : "it was invalidated");
    } else if ((s->bor[i] & B_DANGLING) && v->is_stack) {
        int maybe = s->bor[i] & B_VALID;
        berror(p, "use of %smoved stack object '%s' (%s)", maybe ? "possibly " : "", v->name,
               s->why[i] ? s->why[i] : "it was moved");
    } else if (s->bor[i] & B_DANGLING) {
        int maybe = s->bor[i] & B_VALID;
        berror(p, "borrowed reference '%s' is used after its owner %s%s", v->name, maybe ? "may have been invalidated: " : "",
               s->why[i] ? s->why[i] : "was invalidated");
    } else if ((s->bor[i] & B_CONFLICT) && strncmp(v->name, "__src", 5) == 0) { /* the collection of 'for ... in' (GetEnumerator) */
        berror(p, "the collection of this 'for ... in' loop must not change while the loop runs, but %s",
               s->why[i] ? s->why[i] : "it was changed");
    } else if ((s->bor[i] & B_CONFLICT) && is_case_sel(v)) { /* the selector of a 'case' with patterns */
        berror(p, "the value of this 'case' must not change while its arm is chosen or its names are in use, but %s",
               s->why[i] ? s->why[i] : "it was changed");
    } else if (s->bor[i] & B_CONFLICT) {
        berror(p, "borrowed reference '%s' is used here, but %s", v->name, s->why[i] ? s->why[i] : "its object was used otherwise");
    }
}

/* ---- aliasing rules (like Rust): many read-only borrows OR one changeable borrow ----
 * A borrow conflicts when its object is changed (read-only and changeable borrows) or used
 * another way (changeable borrows) while it is still alive. Like Rust's non-lexical lifetimes
 * the conflict is only an error if the borrow is used afterwards. */
static int in_base; /* >0 while the container part of a place expression is being evaluated */

enum { ACC_READ, ACC_WRITE, ACC_BORROW_MUT, ACC_BORROW_CONST };
static const char *vdisp(Var *v) { /* hidden variables of 'for ... in' and 'case' */
    return strncmp(v->name, "__src", 5) == 0 ? "the loop" : is_case_sel(v) ? "the case" : v->name;
}
static void access_path(State *s, const char *path, Var *ex1, Var *ex2, int kind, Pos p, const char *desc) {
    int write = kind == ACC_WRITE || kind == ACC_BORROW_MUT;
    if (!s->reach || !path) return;
    for (int i = 0; i < tv.len; i++) {
        if (tv.items[i].kind != TV_BORROW) continue;
        Var *v = tv.items[i].v;
        if (v == ex1 || v == ex2) continue;
        int shared = v->type->readonly || ((v->is_alias || v->no_drop) && v->is_const);
        if (!write && shared) continue; /* reading is compatible with read-only borrows */
        for (int k = 0; k < s->org[i].len; k++) {
            Origin *o = &s->org[i].items[k];
            if (!o->path || !paths_overlap(o->path, path)) continue;
            if (o->root == v) continue;
            if (!(s->bor[i] & B_CONFLICT)) { /* report the first conflict */
                if (kind == ACC_WRITE)
                    s->why[i] = sfmt("'%s' was changed at line %d while '%s' was borrowing it%s", desc, p.line, vdisp(v),
                                     shared ? " (a read-only borrow forbids changes)" : "");
                else if (kind == ACC_BORROW_MUT)
                    s->why[i] = sfmt("'%s' was borrowed again as changeable at line %d while '%s' was borrowing it", desc, p.line,
                                     v->name);
                else if (kind == ACC_BORROW_CONST)
                    s->why[i] = sfmt("'%s' was borrowed at line %d while '%s' held a changeable borrow of it", desc, p.line,
                                     v->name);
                else
                    s->why[i] = sfmt("'%s' was used at line %d while '%s' held a changeable borrow of it (borrow it as 'const' to share it)",
                                     desc, p.line, vdisp(v));
            }
            s->bor[i] = (unsigned char)((s->bor[i] & ~B_VALID) | B_CONFLICT);
            break;
        }
    }
}

/* the place e is read (write = 0) or changed (write = 1); ex: borrow doing the access itself */
static void access_expr(Expr *e, State *s, int write, Var *ex) {
    if (!s->reach) return;
    Var *root = NULL;
    char *path = path_of(e, &root);
    if (!path || !root) return;
    int i = slot(root);
    if (i >= 0 && tv.items[i].kind == TV_BORROW) { /* through a borrow: the same part of its objects is accessed */
        const char *rest = path + strlen(sfmt("#%d", root->id)); /* e.g. ".N" of "#7.N" */
        for (int k = 0; k < s->org[i].len; k++) {
            const char *o = s->org[i].items[k].path;
            access_path(s, o ? sfmt("%s%s", o, rest) : NULL, root, ex, write, e->pos, desc_of(e));
        }
        return;
    }
    access_path(s, path, ex, NULL, write, e->pos, desc_of(e));
}

/* ============================================================== nil analysis */

static Expr *strip_conv(Expr *e) {
    while (e->kind == E_CONV || e->kind == E_CAST || e->kind == E_AS) e = e->a;
    return e;
}

static int ctor_field(Expr *e) {
    if (e->kind != E_FIELD || e->a->kind != E_VAR || e->a->var->vkind != VK_SELF) return -1;
    for (int k = 0; k < cfields.len; k++)
        if (cfields.items[k] == e->field) return k;
    return -1;
}

static int is_self_expr(Expr *e) {
    e = e->kind == E_CONV || e->kind == E_CAST ? e->a : e;
    return e->kind == E_VAR && e->var->vkind == VK_SELF;
}

static char *uninit_fields(State *s) {
    Buf b = {0};
    for (int k = 0; k < cfields.len; k++)
        if (s->fd[k] & N_UNINIT) buf_printf(&b, "%s'%s'", b.len ? ", " : "", cfields.items[k]->name);
    return b.d;
}

/* inside a constructor: Self must not escape before all fields are initialized */
static void self_escape_check(State *s, Pos p, const char *how) {
    if (!g_nil_check || !s->reach || !cfields.len) return;
    char *u = uninit_fields(s);
    if (u) nerror(p, "'Self' is used (%s) before the constructor initialized field(s) %s", how, u);
}

/* possible nil states (N_*) of a reference-typed expression */
static unsigned nil_bits(Expr *e, State *s) {
    e = strip_conv(e);
    switch (e->kind) {
    case E_NIL: return N_NIL;
    case E_ADDR: return N_NN;
    case E_VAR: {
        Var *v = e->var;
        if (v->vkind == VK_SELF) return N_NN;
        int i = slot(v);
        if (i >= 0) return s->nn[i];
        return NILABLE(v->type) ? (N_NIL | N_NN) : N_NN;
    }
    case E_RCALL:
        if (e->rt->kind == RK_CTOR) return N_NN;
        break;
    case E_BUILTIN:
        if (e->bi == BI_NEW || e->bi == BI_BOXNEW) return N_NN;
        break;
    case E_FIELD: {
        int k = ctor_field(e);
        if (k >= 0) return s->fd[k];
        break;
    }
    default: break;
    }
    return (e->type && NILABLE(e->type)) ? (N_NIL | N_NN) : N_NN;
}

static char *alloc_hint(Expr *x) {
    const char *d = desc_of(x);
    if (x->kind == E_VAR && x->var->is_stack) return sfmt("%s.Create(...)", d);
    if (x->kind == E_VAR && x->var->is_box) return sfmt("%s := <value>", d);
    if (x->type->kind == T_CLASS) return sfmt("%s := %s.Create", d, x->type->st->name);
    return sfmt("New(%s)", d);
}

/* base is dereferenced here: it must hold an allocated object */
static void deref_check(Expr *base, State *s, Pos p) {
    if (!g_nil_check || !s->reach || !is_ref_type(base->type)) return;
    unsigned b = nil_bits(base, s);
    if (!(b & (N_NIL | N_UNINIT))) return;
    Expr *x = strip_conv(base);
    const char *d = desc_of(x);
    if (x->kind == E_NIL) {
        nerror(p, "dereference of nil");
        return;
    }
    if (ctor_field(x) >= 0) {
        nerror(p, "field '%s' is used before the constructor %s it", x->field->name,
               (b & N_NN) ? "initialized (on every path)" : "initialized");
        return;
    }
    if (x->kind == E_VAR && slot(x->var) >= 0) {
        if ((b & N_UNINIT) && x->var->is_stack)
            nerror(p, "stack object '%s' is used before it %s constructed (missing '%s')", d,
                   (b & N_NN) ? "is always" : "was", alloc_hint(x));
        else if ((b & N_UNINIT) && !(b & (N_NN | N_NIL)))
            nerror(p, "'%s' is used, but no object was allocated for it (missing '%s')", d, alloc_hint(x));
        else if (b & N_UNINIT)
            nerror(p, "'%s' may be used without an allocated object (it is not allocated on every path)", d);
        else if (b & N_NN)
            nerror(p, "'%s' may be nil here; check it with 'if %s <> nil then' before using it", d, d);
        else
            nerror(p, "'%s' is nil here (dereference of nil)", d);
        return;
    }
    nerror(p, "'%s' may be nil (its type is nilable); copy it into a local variable and check it for nil first", d);
}

/* value with nil states 'bits' flows into a location of type tt */
static void nil_flow(Type *tt, Expr *src, unsigned bits, Pos p, const char *tdesc) {
    if (!g_nil_check || !is_ref_type(tt) || NILABLE(tt)) return;
    if (!(bits & (N_NIL | N_UNINIT))) return;
    Expr *x = strip_conv(src);
    const char *d = desc_of(x);
    if (x->kind == E_NIL)
        nerror(p, "cannot assign nil to non-nilable %s (declare it as 'nilable %s')", tdesc, type_name(tt));
    else if (ctor_field(x) >= 0)
        nerror(p, "field '%s' is not initialized yet and cannot be passed to %s", x->field->name, tdesc);
    else if (x->kind == E_VAR && x->var->is_stack && (bits & N_UNINIT))
        nerror(p, "stack object '%s' %s not constructed yet and cannot be passed to %s (missing '%s.Create(...)')", d,
               (bits & N_NN) ? "may be" : "is", tdesc, d);
    else if (x->kind == E_VAR && slot(x->var) >= 0 && (bits & N_UNINIT))
        nerror(p, "'%s' %s no allocated object and cannot be stored in non-nilable %s", d,
               (bits & (N_NN | N_NIL)) ? "may have" : "has", tdesc);
    else
        nerror(p, "possibly nil value '%s' cannot be stored in non-nilable %s; check it for nil first or declare the target 'nilable'",
               d, tdesc);
}

static unsigned settle(unsigned bits) { return (bits & N_UNINIT) ? ((bits & ~N_UNINIT) | N_NIL) : bits; }

/* ============================================================== expression evaluation */

enum { SRC_NONE, SRC_TEMP, SRC_NIL, SRC_OWNER, SRC_BORROW, SRC_PLACE };
typedef struct {
    int kind;
    Var *var;      /* SRC_OWNER */
    Origins org;   /* SRC_BORROW */
    char *path;    /* SRC_PLACE */
    char *desc;
    Var *root;
    int borrowed_place;
    Var *via;      /* the source is reached through this borrow (re-borrow) */
    int temp_base; /* place inside a temporary object */
} Src;

static void use_expr(Expr *e, State *s);
static void exit_checks(State *s, Pos p);
static void use_lvalue(Expr *e, State *s);
static void refine(Expr *c, int truth, State *s);
static Src eval_src(Expr *e, State *s);
static void call_effects(Expr *e, State *s, Origins *out);

static void origin_add(Origins *o, const char *path, const char *desc, Var *root, int stack) {
    if (!path || org_has(o, path)) return;
    Origin x = {(char *)path, (char *)desc, root, stack, NULL};
    vpush(*o, x);
}

static void src_origins(Src *src, Origins *o) {
    switch (src->kind) {
    case SRC_OWNER: origin_add(o, sfmt("#%d", src->var->id), src->var->name, src->var, 0); break;
    case SRC_BORROW:
        for (int k = 0; k < src->org.len; k++) origin_add(o, src->org.items[k].path, src->org.items[k].desc, src->org.items[k].root, src->org.items[k].stack);
        break;
    case SRC_PLACE: origin_add(o, src->path, src->desc, src->root, 0); break;
    default: break;
    }
}

/* does taking the address of this lvalue point into the stack frame? */
/* the borrowed source is a stack object itself (it cannot change its owner) */
static Var *stack_src(Src *src) {
    if (src->kind != SRC_BORROW || src->org.len != 1) return NULL;
    Var *v = src->org.items[0].root;
    if (src->via && src->via != v) return NULL; /* through another borrow, not the stack object itself */
    return v && v->is_stack && STREQ(src->org.items[0].path, sfmt("#%d", v->id)) ? v : NULL;
}

static int addr_is_stack(Expr *e) {
    switch (e->kind) {
    case E_VAR:
        return !e->var->is_global;
    case E_FIELD: return e->a->type->kind == T_CLASS ? 0 : addr_is_stack(e->a);
    case E_INDEX: return addr_is_stack(e->a);
    case E_CAST: case E_CONV: return addr_is_stack(e->a);
    case E_DEREF: return 0;
    default: return 0;
    }
}

static void use_lvalue(Expr *e, State *s) {
    switch (e->kind) {
    case E_VAR:
        if (in_base && tracked_borrow(e->var))
            check_var_use(e->var, e->pos, s); /* a field or element of a borrowed value is used */
        return;
    case E_FIELD:
        in_base++;
        if (e->a->type->kind == T_CLASS) {
            use_expr(e->a, s);
            deref_check(e->a, s, e->pos);
        } else
            use_lvalue(e->a, s);
        in_base--;
        if (!in_base) access_expr(e, s, 0, NULL);
        return;
    case E_INDEX:
        in_base++;
        use_lvalue(e->a, s);
        in_base--;
        use_expr(e->args.items[0], s);
        if (!in_base) access_expr(e, s, 0, NULL);
        return;
    case E_DEREF:
        in_base++;
        use_expr(e->a, s);
        in_base--;
        deref_check(e->a, s, e->pos);
        if (!in_base) access_expr(e, s, 0, NULL);
        return;
    case E_CAST: case E_CONV: use_lvalue(e->a, s); return;
    default: use_expr(e, s); return;
    }
}

static void do_free(Expr *target, State *s, Pos p, int set_nil, const char *how);
static void assign_src(Expr *lhs, Src *src, State *s, Pos p, unsigned nbits);

static int is_free_call(Expr *e) {
    Routine *r = e->rt;
    if (!r->owner || r->owner->kind != SK_CLASS || e->cmode == CM_INHERITED || !e->a) return 0;
    return r->kind == RK_DTOR || r == tobject_free;
}

static void builtin_effects(Expr *e, State *s) {
    ExprVec *A = &e->args;
    switch (e->bi) {
    case BI_NEW: {
        Expr *a = A->items[0];
        if (e->b) {
            Origins o = {0};
            call_effects(e->b, s, &o);
        }
        if (a->kind != E_TYPEREF) {
            Src t = {0};
            t.kind = SRC_TEMP;
            use_lvalue(a, s);
            assign_src(a, &t, s, e->pos, N_NN);
        }
        return;
    }
    case BI_DISPOSE:
        if (e->b) {
            /* destructor call: check args, receiver is consumed below */
            Expr *d = e->b;
            for (int i = 0; i < d->args.len; i++) use_expr(d->args.items[i], s);
        }
        do_free(A->items[0], s, e->pos, 0, "disposed");
        return;
    case BI_BOXFREE: do_free(A->items[0], s, e->pos, 0, "freed"); return;
    case BI_FREEANDNIL: {
        Expr *a = A->items[0];
        if (g_nil_check && !NILABLE(a->type))
            nerror(e->pos, "FreeAndNil would leave non-nilable '%s' nil; declare it as 'nilable %s' or use %s.Free", desc_of(a),
                   type_name(a->type), desc_of(a));
        do_free(a, s, e->pos, 1, "freed");
        if (a->kind == E_VAR && slot(a->var) >= 0) s->nn[slot(a->var)] = N_NIL;
        return;
    }
    case BI_TAKE: {
        Expr *a = A->items[0];
        Var *root = NULL;
        char *path = path_of(a, &root);
        use_lvalue(a, s);
        if (g_nil_check && !NILABLE(a->type) && a->type->kind != T_OPT)
            nerror(e->pos, "Take() would leave '%s' without an object, but it is not optional; declare it as 'optional %s'", desc_of(a),
                   type_name(a->type));
        if (a->kind == E_VAR && slot(a->var) >= 0) s->nn[slot(a->var)] = N_NIL;
        if (a->kind == E_VAR) {
            Var *v = a->var;
            if (v->borrowed) berror(e->pos, "cannot Take() from borrowed reference '%s'", v->name);
            else if (tracked_owner(v)) {
                check_var_use(v, e->pos, s);
                int i = slot(v);
                s->own[i] = O_NULL;
            }
        } else if (a->kind == E_FIELD && a->field->borrowed)
            berror(e->pos, "cannot Take() from borrowed field '%s'", desc_of(a));
        else if (a->kind == E_FIELD)
            use_expr(a, s);
        if (path) invalidate(s, path, sfmt("'%s' was emptied by Take() at line %d", desc_of(a), e->pos.line));
        return;
    }
    case BI_INC: case BI_DEC: case BI_READ: case BI_READLN: case BI_DELETE: case BI_SETLENGTH:
        for (int i = 0; i < A->len; i++) {
            Expr *a = A->items[i];
            if (a->kind == E_VAR || a->kind == E_FIELD || a->kind == E_INDEX || a->kind == E_DEREF) {
                if (i == 0 || e->bi == BI_READ || e->bi == BI_READLN) {
                    use_lvalue(a, s);
                    access_expr(a, s, 1, NULL);
                } else
                    use_expr(a, s);
            } else
                use_expr(a, s);
        }
        if ((e->bi == BI_SETLENGTH || e->bi == BI_DELETE) && A->len && (A->items[0]->type->kind == T_DYN || A->items[0]->type->kind == T_STR)) {
            /* the buffer may move: borrows of its elements are dangling afterwards */
            Var *root = NULL;
            char *path = path_of(A->items[0], &root);
            if (path) invalidate(s, sfmt("%s[]", path), sfmt("'%s' was resized by SetLength at line %d", root ? root->name : desc_of(A->items[0]), e->pos.line));
        }
        return;
    case BI_INSERT:
        use_expr(A->items[0], s);
        use_lvalue(A->items[1], s);
        access_expr(A->items[1], s, 1, NULL);
        use_expr(A->items[2], s);
        return;
    case BI_STR:
        if (A->items[0]->kind == E_FMT) {
            use_expr(A->items[0]->a, s);
            use_expr(A->items[0]->b, s);
            if (A->items[0]->c) use_expr(A->items[0]->c, s);
        } else
            use_expr(A->items[0], s);
        use_lvalue(A->items[1], s);
        return;
    default:
        for (int i = 0; i < A->len; i++) {
            Expr *a = A->items[i];
            if (a->kind == E_FMT) {
                use_expr(a->a, s);
                use_expr(a->b, s);
                if (a->c) use_expr(a->c, s);
            } else if (a->kind != E_TYPEREF)
                use_expr(a, s);
        }
        return;
    }
}

static void use_expr(Expr *e, State *s) {
    if (!e) return;
    switch (e->kind) {
    case E_VAR:
        check_var_use(e->var, e->pos, s);
        if (!in_base) access_expr(e, s, 0, NULL);
        return;
    case E_FIELD: use_lvalue(e, s); return;
    case E_INDEX: use_lvalue(e, s); return;
    case E_DEREF:
        use_expr(e->a, s);
        deref_check(e->a, s, e->pos);
        return;
    case E_ADDR: use_lvalue(e->a, s); return;
    case E_UNARY: case E_CONV: case E_CAST: case E_IS: case E_AS: use_expr(e->a, s); return;
    case E_BINARY:
        if ((e->op == OP_AND || e->op == OP_OR) && e->type->kind == T_BOOL) {
            /* short-circuit: the right operand only runs if the left one is true (and) / false (or) */
            use_expr(e->a, s);
            int need = e->op == OP_AND;
            State rhs = st_copy(s);
            refine(e->a, need, &rhs);
            use_expr(e->b, &rhs);
            State skip = st_copy(s);
            refine(e->a, !need, &skip);
            st_join(&skip, &rhs);
            *s = skip;
            return;
        }
        use_expr(e->a, s);
        use_expr(e->b, s);
        return;
    case E_IN:
        use_expr(e->a, s);
        for (int i = 0; i < e->b->args.len; i++) {
            use_expr(e->b->args.items[i], s);
            use_expr(e->b->args2.items[i], s);
        }
        return;
    case E_RCALL: {
        Origins o = {0};
        call_effects(e, s, &o);
        return;
    }
    case E_BUILTIN: builtin_effects(e, s); return;
    case E_ISSOME: {
        /* 'opt is some k': k receives the content (a borrow of a place, or ownership of a fresh object) */
        Src src = {0};
        if (e->var && tracked_borrow(e->var) && !is_owning_type(e->var->type)) { /* a view of a value that owns objects */
            Origins o = {0};
            Expr *x = e->a;
            while (x->kind == E_CONV) x = x->a;
            if (x->kind == E_RCALL) call_effects(x, s, &o); /* a 'var' result: it points into what the call borrows */
            else {
                use_expr(e->a, s);
                Var *root = NULL;
                char *path = path_of(x, &root);
                if (path && root && tracked_borrow(root)) { /* through another borrow */
                    int j = slot(root);
                    for (int k = 0; k < s->org[j].len; k++) vpush(o, s->org[j].items[k]);
                } else if (path)
                    origin_add(&o, path, desc_of(x), root, 0);
            }
            int i = slot(e->var);
            for (int k = 0; k < o.len; k++) access_path(s, o.items[k].path, e->var, NULL, ACC_BORROW_CONST, e->pos, o.items[k].desc);
            s->bor[i] = B_VALID;
            s->why[i] = NULL;
            s->org[i] = o;
            return;
        }
        if (e->a->type->kind == T_RES && e->var && is_owning_type(e->var->type)) { /* r is ok f: f holds the object of r */
            use_expr(e->a, s);
            src.desc = desc_of(e->a);
            if (res_temp(e->a)) src.kind = SRC_TEMP;
            else {
                src.kind = SRC_PLACE;
                src.path = path_of(e->a, &src.root);
                if (src.root && tracked_borrow(src.root)) src.via = src.root;
            }
        } else
            src = eval_src(e->a, s);
        Var *k = e->var;
        if (slot(k) >= 0) {
            Expr kv = {0};
            kv.kind = E_VAR;
            kv.var = k;
            kv.type = k->type;
            kv.pos = e->pos;
            assign_src(&kv, &src, s, e->pos, N_NN);
        }
        return;
    }
    case E_FMT: use_expr(e->a, s); use_expr(e->b, s); use_expr(e->c, s); return;
    case E_CASEX: use_expr(e->a, s); use_expr(e->b, s); return;
    case E_COND: { /* a ? b : c: one of the two values runs */
        use_expr(e->a, s);
        State t = st_copy(s);
        refine(e->a, 1, &t);
        refine(e->a, 0, s);
        use_expr(e->b, &t);
        use_expr(e->c, s);
        st_join(s, &t);
        return;
    }
    case E_OK: case E_ERR: use_expr(e->a, s); return;
    case E_METHREF: use_expr(e->a, s); return; /* obj.Show as a value: obj must be valid */
    case E_TRY: { /* a failed result leaves the routine here */
        use_expr(e->a, s);
        State out = st_copy(s);
        exit_checks(&out, e->pos);
        return;
    }
    case E_LIST: case E_RECLIT:
        for (int i = 0; i < e->args.len; i++) use_expr(e->args.items[i], s);
        return;
    default: return;
    }
}

static Src eval_src(Expr *e, State *s) {
    Src r = {0};
    while (e->kind == E_CONV || e->kind == E_CAST || e->kind == E_AS) {
        if (e->kind == E_AS || !is_owning_type(e->a->type)) break;
        e = e->a;
    }
    if (e->kind == E_AS) {
        Src inner = eval_src(e->a, s);
        return inner;
    }
    r.desc = desc_of(e);
    switch (e->kind) {
    case E_NIL: r.kind = SRC_NIL; return r;
    case E_CASEX: /* a case expression: the new object of one of its arms */
        if (!e->ival) {
            use_expr(e, s);
            return r;
        }
        use_expr(e->a, s);
        return eval_src(e->b, s);
    case E_COND:
        if (!is_owning_temp(e->b) && e->b->kind != E_NIL && e->b->kind != E_COND) {
            use_expr(e, s);
            return r;
        }
        {
            use_expr(e->a, s);
            State t = st_copy(s);
            refine(e->a, 1, &t);
            refine(e->a, 0, s);
            Src sb = eval_src(e->b, &t);
            Src sc = eval_src(e->c, s);
            st_join(s, &t);
            return sb.kind == SRC_TEMP ? sb : sc;
        }
    case E_RCALL: {
        Origins o = {0};
        call_effects(e, s, &o);
        Routine *rt = e->rt;
        if (!is_owning_type(e->type)) return r;
        if (rt->kind == RK_CTOR) r.kind = e->cmode == CM_NEWCLASS ? SRC_TEMP : SRC_NONE;
        else if (rt->ret_borrowed) {
            r.kind = SRC_BORROW;
            r.org = o;
        } else
            r.kind = SRC_TEMP;
        return r;
    }
    case E_BUILTIN:
        builtin_effects(e, s);
        if (e->bi == BI_TAKE || e->bi == BI_BOXNEW || (e->bi == BI_NEW && is_owning_type(e->type))) r.kind = SRC_TEMP;
        return r;
    case E_VAR: {
        Var *v = e->var;
        if (!is_owning_type(v->type)) {
            use_expr(e, s);
            return r;
        }
        check_var_use(v, e->pos, s);
        if (!in_base) access_expr(e, s, 0, NULL);
        if (tracked_borrow(v)) {
            r.via = v;
            r.kind = SRC_BORROW;
            int i = slot(v);
            for (int k = 0; k < s->org[i].len; k++) vpush(r.org, s->org[i].items[k]);
            return r;
        }
        if (v->borrowed) {
            r.kind = SRC_BORROW;
            origin_add(&r.org, sfmt("#%d", v->id), v->name, v, 0);
            return r;
        }
        r.kind = SRC_OWNER;
        r.var = v;
        return r;
    }
    case E_FIELD: case E_INDEX: case E_DEREF: {
        use_expr(e, s);
        if (!is_owning_type(e->type)) return r;
        r.kind = SRC_PLACE;
        r.path = path_of(e, &r.root);
        if (r.root && tracked_borrow(r.root)) r.via = r.root;
        r.borrowed_place = e->kind == E_FIELD && e->field->borrowed;
        Expr *b = e;
        while (b->kind == E_FIELD || b->kind == E_INDEX || b->kind == E_DEREF || b->kind == E_CONV || b->kind == E_CAST) b = b->a;
        r.temp_base = is_owning_temp(b);
        return r;
    }
    case E_ADDR: {
        use_lvalue(e->a, s);
        r.kind = SRC_BORROW;
        Var *root = NULL;
        char *p = path_of(e->a, &root);
        if (p) origin_add(&r.org, p, desc_of(e->a), root, addr_is_stack(e->a));
        return r;
    }
    default: use_expr(e, s); return r;
    }
}

/* ============================================================== assignments */

static int is_place_kind(Expr *e) {
    return e->kind == E_FIELD || e->kind == E_INDEX || e->kind == E_DEREF || e->kind == E_CAST;
}

/* a local owner may carry references to other objects in its 'var' fields */
static int local_owner(Var *v) {
    int i = v ? slot(v) : -1;
    return i >= 0 && tv.items[i].kind == TV_OWNER && !tv.items[i].varparam && v->vkind != VK_RESULT;
}

/* h.F := r with a 'var' field F: r must live at least as long as the object h.
   Allowed: h lies inside r (a.Child.Parent := a); h is a local owner, which then carries the reference
   (it is checked like a borrow: if r is freed, changed or moved, h may not be used any more) */
static void field_store(Expr *lhs, Origins *o, State *s, Pos p) {
    Var *hroot = NULL;
    char *hp = path_of(lhs->a, &hroot);
    char *lp = path_of(lhs, NULL);
    if (lp && local_owner(hroot)) { /* the field gets a new reference: it forgets the old one */
        int i = slot(hroot);
        Origins keep = {0};
        for (int k = 0; k < s->org[i].len; k++)
            if (!s->org[i].items[k].slot || !STREQ(s->org[i].items[k].slot, lp)) vpush(keep, s->org[i].items[k]);
        s->org[i] = keep;
        if (!keep.len) carry_reset(s, i);
    }
    Origins hold = {0};
    if (hp && hroot && tracked_borrow(hroot)) { /* the holder is reached through a borrow: where that borrow points */
        const char *suffix = hp + strlen(sfmt("#%d", hroot->id));
        int i = slot(hroot);
        for (int k = 0; k < s->org[i].len; k++)
            if (s->org[i].items[k].path) origin_add(&hold, sfmt("%s%s", s->org[i].items[k].path, suffix), NULL, s->org[i].items[k].root, 0);
    } else if (hp)
        origin_add(&hold, hp, NULL, hroot, 0);
    for (int k = 0; k < o->len; k++) {
        Origin *x = &o->items[k];
        if (!x->path) continue;
        int inside = hold.len > 0;
        for (int h = 0; h < hold.len; h++)
            if (!has_prefix(hold.items[h].path, x->path)) inside = 0;
        if (inside) continue; /* the holder is part of the object it refers to */
        Var *r = x->root;
        if (r && r->is_global && slot(r) < 0) continue; /* a global of the program (not tracked here) */
        if (unit && unit->enum_self && hroot && hroot->vkind == VK_RESULT && r && r->vkind == VK_SELF)
            continue; /* GetEnumerator: Result.List := Self (only 'for ... in' uses it, while the container is borrowed) */
        if (local_owner(hroot)) {
            if (local_owner(r) && r->id > hroot->id) {
                berror(p, "'%s' would refer to '%s', which is freed before '%s' (declare '%s' before '%s')", desc_of(lhs), x->desc,
                       hroot->name, r->name, hroot->name);
                continue;
            }
            int i = slot(hroot);
            s->bor[i] |= B_CARRY;
            Origin c = {x->path, x->desc, r, x->stack, lp};
            vpush(s->org[i], c);
            continue;
        }
        berror(p, "cannot store the reference to '%s' in the 'var' field '%s': '%s' could be freed while '%s' still refers to it "
                  "(the object must be part of '%s', or both must be local owners)",
               x->desc, desc_of(lhs), x->desc, desc_of(lhs->a), x->desc);
    }
}

/* a carrier y moves to dest: allowed into the object it refers to, or into another local owner (which then carries) */
static void carrier_move(Var *y, const char *dest, Var *droot, State *s, Pos p, const char *into) {
    int i = y ? slot(y) : -1;
    if (i < 0 || !carrier(s, i)) return;
    for (int k = 0; k < s->org[i].len; k++) {
        Origin *x = &s->org[i].items[k];
        if (!x->path) continue;
        if (dest && has_prefix(dest, x->path)) continue;
        Var *r = x->root;
        if (r && r->is_global && slot(r) < 0) continue;
        if (local_owner(droot) && droot != y) {
            if (local_owner(r) && r->id > droot->id) {
                berror(p, "'%s' refers to '%s' through a 'var' field and cannot be moved into '%s', which is freed after '%s' (declare '%s' before '%s')",
                       y->name, x->desc, droot->name, r->name, r->name, droot->name);
                continue;
            }
            int j = slot(droot);
            s->bor[j] |= B_CARRY | (s->bor[i] & B_DANGLING);
            if (!s->why[j]) s->why[j] = s->why[i];
            origin_add(&s->org[j], x->path, x->desc, r, x->stack);
            continue;
        }
        berror(p, "'%s' refers to '%s' through a 'var' field and cannot be moved into %s, which could outlive '%s'", y->name, x->desc,
               into, x->desc);
    }
    carry_reset(s, i);
}

static void escape_check(Origins *o, Pos p, const char *target) {
    for (int k = 0; k < o->len; k++) {
        Origin *x = &o->items[k];
        if (x->stack)
            berror(p, "reference to local variable '%s' escapes into %s (it would dangle after the routine returns)",
                   x->desc, target);
        else if (x->root && !x->root->is_global && slot(x->root) >= 0 && tv.items[slot(x->root)].kind == TV_OWNER &&
                 !tv.items[slot(x->root)].varparam && x->root->vkind != VK_RESULT)
            berror(p, "reference to '%s' escapes into %s, but '%s' frees its object at the end of %s", x->desc, target,
                   x->root->name, unit_name);
    }
}

static void move_out(Src *src, State *s, Pos p, const char *into) {
    Var *y = src->var;
    if (tracked_owner(y)) {
        int i = slot(y);
        s->own[i] = O_MOVED;
        invalidate(s, sfmt("#%d", y->id), sfmt("'%s' was moved into %s at line %d", y->name, into, p.line));
    } else if (!y->is_ref || y->vkind != VK_PARAM)
        berror(p, "cannot move out of non-local variable '%s'; use Take(%s) to transfer ownership", y->name, y->name);
}

static void assign_src(Expr *lhs, Src *src, State *s, Pos p, unsigned nbits) {
    Var *root = NULL;
    if (lhs->kind == E_VAR && slot(lhs->var) >= 0) s->nn[slot(lhs->var)] = (unsigned char)settle(nbits);
    char *lpath = path_of(lhs, &root);
    char *ldesc = desc_of(lhs);
    int target_borrow = 0;
    Var *tvar = lhs->kind == E_VAR ? lhs->var : NULL;
    if (tvar) target_borrow = tvar->borrowed;
    else if (lhs->kind == E_FIELD) target_borrow = lhs->field->borrowed;

    if (!target_borrow) {
        switch (src->kind) {
        case SRC_OWNER:
            if (tvar && src->var == tvar) return;
            carrier_move(src->var, lpath, root, s, p, sfmt("'%s'", ldesc));
            move_out(src, s, p, sfmt("'%s'", ldesc));
            break;
        case SRC_BORROW:
            if (stack_src(src)) { /* the stack object moves to the heap; it is dead afterwards */
                Var *k = stack_src(src);
                invalidate(s, sfmt("#%d", k->id), sfmt("'%s' was moved to the heap ('%s') at line %d", k->name, ldesc, p.line));
            } else
                berror(p, "cannot store borrowed reference '%s' in owning %s '%s'%s", src->desc, tvar ? "variable" : "location",
                       ldesc, tvar ? sfmt("; declare '%s' with 'var' (reference) or transfer ownership", ldesc) : "");
            break;
        case SRC_PLACE:
            if (src->borrowed_place)
                berror(p, "cannot store borrowed reference '%s' in owning %s '%s'", src->desc, tvar ? "variable" : "location", ldesc);
            else
                berror(p, "cannot move out of '%s' (it is owned by its container); use Take(%s)%s", src->desc, src->desc,
                       tvar ? sfmt(" or declare '%s' with 'var' (reference)", ldesc) : "");
            break;
        default: break;
        }
        if (tvar && tracked_owner(tvar)) {
            int i = slot(tvar);
            s->own[i] = src->kind == SRC_NIL ? O_NULL : O_OWNED;
            if (!(src->kind == SRC_OWNER && local_owner(src->var) && carrier(s, i))) carry_reset(s, i); /* a new object */
        }
        if (lpath) invalidate(s, lpath, sfmt("'%s' was reassigned at line %d", ldesc, p.line));
        return;
    }
    /* target is a borrow */
    Origins o = {0};
    if (src->kind == SRC_PLACE && src->temp_base)
        berror(p, "'%s' would refer into a temporary object, which is freed at the end of the statement; store the object in a variable first",
               ldesc);
    switch (src->kind) {
    case SRC_TEMP:
        berror(p, "an owned temporary (%s) cannot be stored in borrowed '%s'; it would never be freed", src->desc, ldesc);
        break;
    case SRC_NIL: case SRC_NONE: break;
    default: src_origins(src, &o); break;
    }
    if (tvar && tracked_borrow(tvar)) {
        int i = slot(tvar);
        for (int k = 0; k < o.len; k++)
            access_path(s, o.items[k].path, tvar, src->via, (tvar->type->readonly || (tvar->is_alias && tvar->is_const)) ? ACC_BORROW_CONST : ACC_BORROW_MUT, p,
                        o.items[k].desc);
        s->bor[i] = B_VALID;
        s->why[i] = NULL;
        s->org[i] = o;
        return;
    }
    if (lhs->kind == E_FIELD && lhs->field->borrowed) { /* a 'var' field */
        field_store(lhs, &o, s, p);
        return;
    }
    /* borrowed field, borrowed global or other untracked borrow */
    if (!tvar || tvar->is_global || tvar->level < (unit ? unit->level : 0))
        escape_check(&o, p, sfmt("'%s'", ldesc));
}

/* var x: T := y: x borrows y (read-only with 'T const') until its last use */
static void do_bind(Var *v, Expr *rhs, State *s, Pos p) {
    in_base++;
    use_expr(rhs, s);
    in_base--;
    int i = slot(v);
    if (i < 0) return;
    Var *root = NULL;
    char *path = path_of(rhs, &root);
    Origins o = {0};
    if (path && root) {
        int j = slot(root);
        if (j >= 0 && tv.items[j].kind == TV_BORROW && root != v) { /* through another borrow: re-borrow */
            check_var_use(root, p, s);
            /* a part of a value alias (var r := alias.Field): the same part of what the alias refers to */
            const char *suffix = root->is_alias ? path + strlen(sfmt("#%d", root->id)) : "";
            for (int k = 0; k < s->org[j].len; k++) {
                Origin og = s->org[j].items[k];
                if (*suffix && og.path) {
                    og.path = sfmt("%s%s", og.path, suffix);
                    og.desc = desc_of(rhs);
                }
                vpush(o, og);
            }
        } else
            origin_add(&o, path, desc_of(rhs), root, !root->is_global);
    }
    int kind = (v->type->readonly || (v->is_alias && v->is_const)) ? ACC_BORROW_CONST : ACC_BORROW_MUT;
    for (int k = 0; k < o.len; k++) access_path(s, o.items[k].path, v, root, kind, p, o.items[k].desc);
    s->nn[i] = nil_bits(rhs, s); /* the reference holds what the place holds */
    s->bor[i] = B_VALID;
    s->why[i] = NULL;
    s->org[i] = o;
}

/* val b := k: the stack object k moves into b; k and every borrow of it are dead afterwards */
static void do_stack_move(Var *b, Var *k, State *s, Pos p) {
    check_var_use(k, p, s);
    invalidate(s, sfmt("#%d", k->id), sfmt("'%s' was moved to '%s' at line %d", k->name, b->name, p.line));
    int i = slot(b);
    if (i >= 0) {
        s->nn[i] = N_NN;
        s->bor[i] = B_VALID;
        s->why[i] = NULL;
    }
}

/* val k := h: an owned heap object moves into the stack object k */
static void do_heap_to_stack(Var *k, Expr *rhs, State *s, Pos p) {
    Src src = eval_src(rhs, s);
    switch (src.kind) {
    case SRC_OWNER: move_out(&src, s, p, sfmt("stack object '%s'", k->name)); break;
    case SRC_BORROW:
        berror(p, "'%s' is a reference and does not own its object, so it cannot be moved into stack object '%s'; move it from its owner, or copy it ('val %s := %s.Clone')",
               src.desc, k->name, k->name, src.desc);
        break;
    case SRC_PLACE:
        if (src.borrowed_place)
            berror(p, "'%s' is a reference and does not own its object, so it cannot be moved into stack object '%s'", src.desc,
                   k->name);
        else
            berror(p, "cannot move '%s' out of its container into stack object '%s'; take it out first ('if Take(%s) is some x then ...') or copy it ('.Clone')",
                   src.desc, k->name, src.desc);
        break;
    default: break;
    }
    int i = slot(k);
    if (i >= 0) {
        s->nn[i] = N_NN;
        s->bor[i] = B_VALID;
        s->why[i] = NULL;
    }
}

static void do_assign(Expr *lhs, Expr *rhs, State *s, Pos p) {
    if (lhs->kind == E_DEREF && lhs->a->kind == E_VAR && lhs->a->var->is_box) {
        /* storing into a heap value: allocates the cell if it has none yet */
        use_expr(rhs, s);
        access_expr(lhs, s, 1, NULL);
        int i = slot(lhs->a->var);
        if (i >= 0) {
            s->own[i] = O_OWNED;
            s->nn[i] = N_NN;
        }
        return;
    }
    if (!is_owning_type(lhs->type)) {
        /* 'Result := ok(f)': a result that owns its object takes it from f */
        Expr *in = rhs;
        while ((in->kind == E_OK || in->kind == E_CONV) && in->a) in = in->a;
        if (in != rhs && lhs->type->kind == T_RES && is_owning_type(in->type)) {
            Src src = eval_src(in, s);
            if (src.kind == SRC_OWNER) {
                Var *lr = NULL;
                char *lp = path_of(lhs, &lr);
                carrier_move(src.var, lp, lr, s, p, sfmt("'%s'", desc_of(lhs)));
                move_out(&src, s, p, sfmt("'%s'", desc_of(lhs)));
            } else if (src.kind == SRC_PLACE)
                berror(p, "cannot move out of '%s' (it is owned by its container); use Take(%s)", src.desc, src.desc);
            else if (src.kind == SRC_BORROW && !stack_src(&src))
                berror(p, "cannot store borrowed reference '%s' in the result '%s'", src.desc, desc_of(lhs));
            use_lvalue(lhs, s);
            access_expr(lhs, s, 1, NULL);
            return;
        }
        use_expr(rhs, s);
        use_lvalue(lhs, s);
        if (lhs->kind == E_VAR && lhs->var->is_alias) check_var_use(lhs->var, lhs->pos, s); /* writing through a reference */
        access_expr(lhs, s, 1, NULL);
        return;
    }
    /* binding a borrow to a place: the borrow itself is the access (reported as borrow, not as read) */
    Expr *pl = strip_conv(rhs);
    int rebind = lhs->kind == E_VAR && tracked_borrow(lhs->var) &&
                 (pl->kind == E_VAR || pl->kind == E_FIELD || pl->kind == E_INDEX || pl->kind == E_DEREF);
    if (rebind) in_base++;
    Src src = eval_src(rhs, s);
    if (rebind) in_base--;
    unsigned nb = nil_bits(rhs, s);
    use_lvalue(lhs, s);
    (void)is_place_kind;
    nil_flow(lhs->type, rhs, nb, p, sfmt("'%s'", desc_of(lhs)));
    if (is_self_expr(rhs)) self_escape_check(s, p, sfmt("stored in '%s'", desc_of(lhs)));
    if (!(lhs->kind == E_VAR && tracked_borrow(lhs->var))) access_expr(lhs, s, 1, NULL); /* rebinding a borrow is no change */
    assign_src(lhs, &src, s, p, nb);
    int k = ctor_field(lhs);
    if (k >= 0) s->fd[k] = N_NN;
}

static void do_free(Expr *t, State *s, Pos p, int set_nil, const char *how) {
    Var *root = NULL;
    char *path = path_of(t, &root);
    if (t->kind == E_VAR) {
        Var *v = t->var;
        if (v->vkind == VK_SELF) return;
        if (v->is_stack) {
            berror(p, "'%s' is a stack object; it is destroyed automatically at the end of its block and must not be freed",
                   v->name);
            return;
        }
        if (v->borrowed) {
            berror(p, "cannot free borrowed reference '%s' (it does not own the object)", v->name);
            return;
        }
        if (tracked_owner(v)) {
            int i = slot(v);
            unsigned st = s->own[i];
            int maybe = (st & (O_NULL | O_OWNED | O_UNK)) != 0;
            if (s->reach) {
                if (st & O_MOVED)
                    berror(p, "cannot free '%s': its value was %smoved away", v->name, maybe ? "possibly " : "");
                if (st & O_FREED) berror(p, "'%s' is %sfreed twice (double free)", v->name, maybe ? "possibly " : "");
            }
            s->own[i] = set_nil ? O_NULL : O_FREED;
        }
    } else {
        use_lvalue(t, s);
        if (t->kind == E_FIELD && t->field->borrowed)
            berror(p, "cannot free borrowed field '%s' (it does not own the object)", desc_of(t));
    }
    if (path) invalidate(s, path, sfmt("'%s' was %s at line %d", desc_of(t), how, p.line));
}

/* ============================================================== calls */

static void writes_effect(Routine *r, State *s) {
    if (!r || r->prelude) return;
    VarSet *w = &wsets[r->id];
    for (int k = 0; k < w->len; k++) {
        Var *v = w->items[k];
        int i = slot(v);
        if (i < 0) continue;
        s->nn[i] = NILABLE(v->type) ? (N_NIL | N_NN) : N_NN;
        if (tv.items[i].kind == TV_OWNER) {
            s->own[i] = O_UNK;
            invalidate_root(s, v, sfmt("'%s' may have been changed by the call to '%s'", v->name, r->name));
        } else {
            s->bor[i] = B_VALID;
            s->org[i].len = 0;
        }
    }
}

static Field *own_nonnil_field(Struct *x) {
    for (int i = 0; i < x->fields.len; i++) {
        Field *f = x->fields.items[i];
        if (f->owner == x && is_ref_type(f->type) && !NILABLE(f->type)) return f;
    }
    return NULL;
}

/* T is created/constructed with ctor: every class between T and the ctor's class must be covered */
static void creation_check(Type *T, Routine *ctor, Pos p) {
    if (!g_nil_check) return;
    for (Struct *x = T->st; x && x != ctor->owner; x = x->parent) {
        Field *f = own_nonnil_field(x);
        if (f) {
            nerror(p, "'%s' cannot be created with the inherited constructor '%s.%s': it does not initialize field '%s' of '%s'",
                   T->st->name, ctor->owner->name, ctor->name, f->name, x->name);
            return;
        }
    }
}

static void reads_check(Routine *r, State *s, Pos p) {
    if (!s->reach || !r || r->prelude) return;
    VarSet *rs = &rsets[r->id];
    for (int k = 0; k < rs->len; k++) { /* the routine (a nested routine, a closure) uses variables of this one */
        Var *v = rs->items[k];
        int i = slot(v);
        if (i < 0) continue;
        if (tv.items[i].kind == TV_OWNER && (s->own[i] & (O_FREED | O_MOVED)))
            berror(p, "the call %s uses '%s' after it %s %s", r->is_sig ? "through a function value (a closure)" : sfmt("of '%s'", r->name), v->name, (s->own[i] & (O_OWNED | O_UNK)) ? "may have been" : "was",
                   (s->own[i] & O_FREED) ? "freed" : "moved away");
        else if (tv.items[i].kind == TV_BORROW && (s->bor[i] & B_DANGLING))
            berror(p, "the call of '%s' uses the borrowed reference '%s', but %s", r->name, v->name, s->why[i] ? s->why[i] : "its owner is gone");
    }
    if (!g_nil_check) return;
    for (int k = 0; k < rs->len; k++) {
        Var *v = rs->items[k];
        int i = slot(v);
        if (i >= 0 && (s->nn[i] & (N_UNINIT | N_NIL)))
            nerror(p, "the call of '%s' %s '%s' before %s", r->name, (s->nn[i] & N_NN) ? "may use" : "uses", v->name,
                   v->is_stack ? "it is constructed" : "an object was assigned to it");
    }
}

typedef struct {
    char *path;
    char *desc;
    int mut;
    int shared;
} ArgPath;

/* the result of a function with a 'var' result may point into this argument (an element or a field of it) */
static void borrowed_from_arg(Expr *a, const char *path, Var *root, Origins *out) {
    Type *t = a->type;
    if (t->kind == T_DYN || t->kind == T_ARRAY) origin_add(out, sfmt("%s[]", path), sfmt("%s[...]", desc_of(a)), root, 0);
    else if (t->kind == T_RECORD) origin_add(out, path, desc_of(a), root, 0);
}

static void call_effects_inner(Expr *e, State *s, Origins *out);
static void call_effects(Expr *e, State *s, Origins *out) {
    if (e->cmode == CM_INDIRECT && e->b && e->b->kind == E_VAR) check_var_use(e->b->var, e->b->pos, s); /* f; with f := c.Tick */
    if (e->rt && e->rt->enum_self && !e->forin)
        berror(e->pos, "'%s.GetEnumerator' gives an enumerator that refers to the %s: only 'for ... in' can use it",
               e->rt->owner->name, e->rt->owner->name);
    call_effects_inner(e, s, out);
}
static void call_effects_inner(Expr *e, State *s, Origins *out) {
    Routine *r = e->rt;
    VEC(ArgPath) ap = {0};
    if (unit && unit->kind == RK_CTOR && e->a && is_self_expr(e->a)) {
        if (e->cmode == CM_INHERITED && r->kind == RK_CTOR) s->fd[FD_INH] = N_NN;
        else self_escape_check(s, e->pos, sfmt("call of '%s'", r->name));
    }
    if (e->cmode == CM_NEWCLASS) creation_check(e->ctype, r, e->pos);
    if (e->cmode == CM_CTOR_INST && e->a && e->a->kind == E_VAR && e->a->var->is_stack)
        creation_check(e->a->var->type, r, e->pos);
    for (int i = 0; i < e->args.len; i++)
        if (is_self_expr(e->args.items[i])) self_escape_check(s, e->args.items[i]->pos, sfmt("passed to '%s'", r->name));
    if (e->a) {
        int stack_ctor = e->cmode == CM_CTOR_INST && e->a->kind == E_VAR && e->a->var->is_stack;
        if (stack_ctor) {
            int k = slot(e->a->var);
            if (g_nil_check && s->reach && k >= 0 && (s->nn[k] & N_NN))
                nerror(e->pos, "stack object '%s' is %sconstructed twice", e->a->var->name,
                       (s->nn[k] & N_UNINIT) ? "possibly " : "");
            if (k >= 0) s->nn[k] = N_NN;
        } else if (e->a->type->kind == T_CLASS && r != tobject_free) {
            deref_check(e->a, s, e->pos);
        }
        if (is_free_call(e)) {
            do_free(e->a, s, e->pos, 0, "freed");
        } else {
            Src rs = {0};
            if (e->a->type->kind == T_CLASS) rs = eval_src(e->a, s);
            else use_lvalue(e->a, s);
            if (r->ret_borrowed && rs.kind == SRC_TEMP)
                berror(e->pos, "the result of '%s' would borrow from a temporary object, which is freed at the end of the statement; store it in a variable first",
                       r->name);
            if (r->ret_borrowed) src_origins(&rs, out);
            if (e->a->type->kind != T_CLASS) {
                Var *root;
                ArgPath a = {path_of(e->a, &root), desc_of(e->a), 0, 1};
                if (a.path) vpush(ap, a);
            }
        }
    }
    for (int i = 0; i < e->args.len; i++) {
        Param *pm = &r->params.items[i];
        Expr *a = e->args.items[i];
        Var *root = NULL;
        if (pm->mode == PM_VAR || pm->mode == PM_OUT) {
            use_lvalue(a, s);
            if (g_nil_check && is_ref_type(pm->type)) {
                if (NILABLE(pm->type) != NILABLE(a->type))
                    nerror(a->pos, "var argument '%s' (%s) must have the same nilability as parameter '%s' (%s)", desc_of(a),
                           type_name(a->type), pm->name, type_name(pm->type));
                else if (pm->mode == PM_VAR && !NILABLE(pm->type))
                    nil_flow(pm->type, a, nil_bits(a, s), a->pos, sfmt("var parameter '%s' of '%s'", pm->name, r->name));
            }
            ArgPath x = {path_of(a, &root), desc_of(a), 1, 0};
            if (x.path) vpush(ap, x);
            if (r->ret_borrowed && x.path) borrowed_from_arg(a, x.path, root, out);
            continue;
        }
        if (is_owning_type(pm->type)) {
            Src src = eval_src(a, s);
            nil_flow(pm->type, a, nil_bits(a, s), a->pos, sfmt("parameter '%s' of '%s'", pm->name, r->name));
            if (pm->mode == PM_OWNED) {
                switch (src.kind) {
                case SRC_OWNER: {
                    Var *rr = NULL; /* into a method of the object it refers to (parent.Add(child)): stays inside it */
                    char *rp = e->a ? path_of(e->a, &rr) : NULL;
                    if (rp && rr && tracked_borrow(rr)) rp = NULL;
                    carrier_move(src.var, rp, NULL, s, a->pos, sfmt("parameter '%s' of '%s'", pm->name, r->name));
                    move_out(&src, s, a->pos, sfmt("parameter '%s' of '%s'", pm->name, r->name));
                    break;
                }
                case SRC_BORROW: case SRC_PLACE:
                    if (src.kind == SRC_PLACE && !src.borrowed_place)
                        berror(a->pos, "cannot move out of '%s' into owned parameter '%s'; use Take(%s)", src.desc, pm->name,
                               src.desc);
                    else if (stack_src(&src)) { /* moves to the heap, owned by the parameter */
                        Var *k = stack_src(&src);
                        invalidate(s, sfmt("#%d", k->id),
                                   sfmt("'%s' was moved to the heap (parameter '%s' of '%s') at line %d", k->name, pm->name,
                                        r->name, a->pos.line));
                    } else
                        berror(a->pos, "cannot pass borrowed reference '%s' to owned parameter '%s' of '%s'", src.desc,
                               pm->name, r->name);
                    break;
                default: break;
                }
            } else {
                if (r->ret_borrowed && src.kind == SRC_TEMP)
                    berror(a->pos, "the result of '%s' would borrow from a temporary object, which is freed at the end of the statement; store '%s' in a variable first",
                           r->name, src.desc);
                if (r->ret_borrowed) src_origins(&src, out);
                char *p = path_of(a, &root);
                if (p) {
                    ArgPath x = {p, desc_of(a), 0, 1};
                    vpush(ap, x);
                }
            }
        } else {
            if (r->ret_borrowed) { /* a 'var' result may refer into an array or record argument */
                Var *ar = NULL;
                char *apth = path_of(a, &ar);
                if (apth) borrowed_from_arg(a, apth, ar, out);
            }
            use_expr(a, s);
            if (pm->mode == PM_CONST || pm->mode == PM_CONSTREF) {
                char *p = path_of(a, &root);
                if (p) {
                    ArgPath x = {p, desc_of(a), 0, 1};
                    vpush(ap, x);
                }
            }
        }
    }
    /* a method value in the arguments: its object is used during the call; it may not be moved or changed by another argument */
    for (int i = 0; i < e->args.len; i++) {
        Expr *mv = e->args.items[i];
        while (mv->kind == E_CONV) mv = mv->a;
        if (mv->kind != E_METHREF) continue;
        Var *mroot = NULL;
        char *mp = path_of(mv->a, &mroot);
        if (!mp) continue;
        for (int k = 0; k < e->args.len; k++) {
            if (k == i) continue;
            int mode = r->params.items[k].mode;
            Var *aroot = NULL;
            char *apth = path_of(e->args.items[k], &aroot);
            if (!apth || !paths_overlap(apth, mp)) continue;
            if (mode == PM_OWNED || mode == PM_VAR || mode == PM_OUT)
                berror(e->args.items[k]->pos, "'%s' is %s in the call of '%s' while the method value '%s.%s' uses it", desc_of(e->args.items[k]),
                       mode == PM_OWNED ? "moved" : "passed as var", r->name, desc_of(mv->a), mv->rt->name);
        }
    }
    /* aliasing of mutable borrows */
    for (int i = 0; i < ap.len; i++)
        for (int j = i + 1; j < ap.len; j++) {
            ArgPath *x = &ap.items[i], *y = &ap.items[j];
            if (!x->mut && !y->mut) continue;
            if (!paths_overlap(x->path, y->path)) continue;
            if (x->mut && y->mut)
                berror(e->pos, "cannot borrow '%s' as mutable (var) more than once in the same call of '%s'", y->desc, r->name);
            else
                berror(e->pos, "cannot borrow '%s' as mutable (var) because it is also borrowed in the same call of '%s'",
                       x->mut ? x->desc : y->desc, r->name);
        }
    /* aliasing: a changing method changes its receiver; var/out arguments are changed */
    if (e->a && !r->is_constm && e->cmode != CM_NEWCLASS && !is_free_call(e)) access_expr(e->a, s, 1, NULL);
    for (int i = 0; i < e->args.len; i++) {
        int m = r->params.items[i].mode;
        if (m == PM_VAR || m == PM_OUT) access_expr(e->args.items[i], s, 1, NULL);
    }
    /* effects of var parameters */
    for (int i = 0; i < e->args.len; i++) {
        Param *pm = &r->params.items[i];
        if (pm->mode != PM_VAR && pm->mode != PM_OUT) continue;
        Expr *a = e->args.items[i];
        if (!is_owning_type(a->type)) continue;
        Var *root = NULL;
        char *p = path_of(a, &root);
        if (a->kind == E_VAR && slot(a->var) >= 0) s->nn[slot(a->var)] = NILABLE(pm->type) ? (N_NIL | N_NN) : N_NN;
        if (a->kind == E_VAR && tracked_owner(a->var)) s->own[slot(a->var)] = O_UNK;
        else if (a->kind == E_VAR && tracked_borrow(a->var)) {
            int k = slot(a->var);
            s->bor[k] = B_VALID;
            s->org[k].len = 0;
        }
        if (p) invalidate(s, p, sfmt("'%s' was passed as var parameter to '%s' at line %d", desc_of(a), r->name, e->pos.line));
    }
    /* effects on non-local variables */
    if (e->cmode == CM_VIRTUAL || e->cmode == CM_IFACE) {
        for (int i = 0; i < prog->routines.len; i++) {
            Routine *q = prog->routines.items[i];
            if (q->owner && STREQ(q->name, r->name)) reads_check(q, s, e->pos);
        }
        for (int i = 0; i < prog->routines.len; i++) {
            Routine *q = prog->routines.items[i];
            if (q->owner && STREQ(q->name, r->name)) writes_effect(q, s);
        }
    } else {
        reads_check(r, s, e->pos);
        writes_effect(r, s);
    }
}

/* ============================================================== statements */

static void an_stmt(Stmt *st, State *s);

static void an_list(StmtVec *v, State *s) {
    for (int i = 0; i < v->len; i++) an_stmt(v->items[i], s);
}

/* refine state by a condition (x <> nil, Assigned(x), not ...) */
static void refine(Expr *c, int truth, State *s) {
    if (!s->reach) return;
    if (c->kind == E_UNARY && c->op == OP_NOT) {
        refine(c->a, !truth, s);
        return;
    }
    if (c->kind == E_BINARY && c->type->kind == T_BOOL && (c->op == OP_AND || c->op == OP_OR)) {
        int both = c->op == OP_AND ? truth : !truth; /* a and b true / a or b false: both sides hold */
        if (both) {
            refine(c->a, truth, s);
            refine(c->b, truth, s);
        } else {
            /* a and b false: a false, or a true and b false (or analogously for 'or') */
            State alt = st_copy(s);
            refine(c->a, truth, s);
            refine(c->a, c->op == OP_AND, &alt);
            refine(c->b, truth, &alt);
            st_join(s, &alt);
        }
        return;
    }
    Var *v = NULL;
    int nonnil_if_true = 1;
    if (c->kind == E_BINARY && (c->op == OP_NE || c->op == OP_EQ)) {
        Expr *x = c->a, *y = c->b;
        while (x->kind == E_CONV) x = x->a;
        while (y->kind == E_CONV) y = y->a;
        if (y->kind == E_NIL && x->kind == E_VAR) v = x->var;
        else if (x->kind == E_NIL && y->kind == E_VAR) v = y->var;
        nonnil_if_true = c->op == OP_NE;
    } else if (c->kind == E_BUILTIN && (c->bi == BI_ASSIGNED || c->bi == BI_HASVALUE) && c->args.items[0]->kind == E_VAR)
        v = c->args.items[0]->var;
    else if (c->kind == E_ISSOME) {
        /* the binding only holds something when the test succeeded */
        int k = slot(c->var);
        if (k >= 0) {
            if (truth) {
                s->nn[k] = N_NN;
                if (tv.items[k].kind == TV_OWNER && (s->own[k] & ~O_NULL)) s->own[k] &= (unsigned char)~O_NULL;
            } else {
                s->nn[k] = N_UNINIT;
                if (tv.items[k].kind == TV_OWNER) s->own[k] = O_NULL;
            }
        }
        Expr *x = strip_conv(c->a);
        if (x->kind == E_VAR) v = x->var;
    }
    if (!v || slot(v) < 0) return;
    int i = slot(v);
    int nonnil = truth ? nonnil_if_true : !nonnil_if_true;
    /* a branch that contradicts what is known about v cannot be taken */
    if (nonnil) {
        if (!(s->nn[i] & N_NN)) {
            s->reach = 0;
            return;
        }
        s->nn[i] = N_NN;
    } else {
        if (!(s->nn[i] & (N_NIL | N_UNINIT))) {
            s->reach = 0;
            return;
        }
        s->nn[i] &= N_NIL | N_UNINIT;
    }
    if (!tracked_owner(v)) return;
    if (nonnil) {
        if (s->own[i] & ~O_NULL) s->own[i] &= (unsigned char)~O_NULL;
    } else
        s->own[i] = O_NULL;
}

static void exit_checks(State *s, Pos p) {
    if (!s->reach) return;
    const char *where = unit_name;
    for (int k = 0; k < cfields.len; k++)
        if (s->fd[k] & N_UNINIT)
            nerror(p, "constructor '%s' does not initialize non-nilable field '%s'%s", where, cfields.items[k]->name,
                   (s->fd[k] & N_NN) ? " on every path" : "");
    if (cneed_inh && (s->fd[FD_INH] & N_UNINIT))
        nerror(p, "constructor '%s' must call an inherited constructor%s (the parent class has non-nilable fields)", where,
               (s->fd[FD_INH] & N_NN) ? " on every path" : "");
    for (int i = 0; i < tv.len; i++) {
        Var *v = tv.items[i].v;
        if (!NILABLE(v->type) && (s->nn[i] & (N_NIL | N_UNINIT))) {
            int sure = !(s->nn[i] & N_NN);
            if (v->vkind == VK_RESULT)
                nerror(p, "'%s' %s, but its result type '%s' is not nilable", where,
                       !sure ? "may return nil or no object" : (s->nn[i] & N_UNINIT) ? "never assigns an object to its result" : "returns nil",
                       type_name(v->type));
            else if (tv.items[i].varparam && v->mode == PM_OUT && (s->nn[i] & N_UNINIT))
                nerror(p, "out parameter '%s' is %snot assigned when '%s' returns", v->name, sure ? "" : "possibly ", where);
            else if (tv.items[i].varparam)
                nerror(p, "var parameter '%s' %s nil when '%s' returns, but its type is not nilable", v->name,
                       sure ? "is" : "may be", where);
        }
        if (tv.items[i].kind == TV_OWNER && carrier(s, i) && (s->own[i] & (O_OWNED | O_UNK))) {
            if (v->vkind == VK_RESULT || tv.items[i].varparam) {
                for (int k = 0; k < s->org[i].len; k++) {
                    Var *r = s->org[i].items[k].root;
                    if (r && slot(r) >= 0 && tv.items[slot(r)].kind == TV_OWNER && !tv.items[slot(r)].varparam && r->vkind != VK_RESULT)
                        berror(p, "%s refers to '%s' through a 'var' field, but '%s' frees its object when '%s' returns",
                               v->vkind == VK_RESULT ? "the function result" : sfmt("var parameter '%s'", v->name),
                               s->org[i].items[k].desc, r->name, where);
                }
            } else if (s->bor[i] & B_DANGLING)
                berror(p, "'%s' still refers to another object through a 'var' field when it is freed at the end of %s, but %s", v->name,
                       where, s->why[i] ? s->why[i] : "that object is gone");
        }
        if (tv.items[i].kind == TV_OWNER) {
            unsigned st = s->own[i];
            if (v->vkind == VK_RESULT) {
                if (st & (O_MOVED | O_FREED))
                    berror(p, "function result is %s%s before '%s' returns", (st & ~(O_MOVED | O_FREED)) ? "possibly " : "",
                           (st & O_FREED) ? "freed" : "moved away", where);
                continue;
            }
            if (tv.items[i].varparam) {
                if (st & (O_MOVED | O_FREED))
                    berror(p, "var parameter '%s' is left %s when '%s' returns; assign a new value or nil", v->name,
                           (st & O_FREED) ? "freed" : "moved", where);
                continue;
            }
        } else if (v->vkind == VK_RESULT) {
            for (int k = 0; k < s->org[i].len; k++) {
                Origin *o = &s->org[i].items[k];
                if (o->stack)
                    berror(p, "'%s' returns a reference to local variable '%s'", where, o->desc);
                else if (o->root && tracked_owner(o->root) && slot(o->root) >= 0 &&
                         !tv.items[slot(o->root)].varparam && o->root->vkind != VK_RESULT)
                    berror(p, "'%s' returns a reference to '%s', which is owned by local variable '%s'", where, o->desc,
                           o->root->name);
            }
            if (s->bor[i] & B_DANGLING)
                berror(p, "'%s' returns a dangling reference: %s", where, s->why[i] ? s->why[i] : "its owner was invalidated");
        }
    }
}

static void loop_body(Stmt *st, State *s, int kind) {
    /* kind: 0 while (cond first), 1 repeat (cond last), 2 for */
    State head = st_copy(s);
    State exitst = st_unreach();
    for (int iter = 0; iter < 20; iter++) {
        LoopCtx *lc = NEW(LoopCtx);
        lc->blk = cblocks.len;
        lc->brk = st_unreach();
        lc->cont = st_unreach();
        vpush(loops, lc);
        State b = st_copy(&head);
        if (kind == 0 && !st->a) { /* loop: only Break leaves it */
            an_stmt(st->s1, &b);
        } else if (kind == 0) {
            use_expr(st->a, &b);
            State ex = st_copy(&b);
            refine(st->a, 0, &ex);
            exitst = ex;
            refine(st->a, 1, &b);
            an_stmt(st->s1, &b);
        } else if (kind == 1) {
            an_list(&st->body, &b);
        } else {
            exitst = st_copy(&b);
            an_stmt(st->s1, &b);
        }
        st_join(&b, &lc->cont);
        if (kind == 1) {
            use_expr(st->a, &b);
            exitst = st_copy(&b);
        }
        loops.len--;
        st_join(&exitst, &lc->brk);
        State nh = st_copy(&head);
        st_join(&nh, &b);
        if (kind == 2) st_join(&exitst, &b);
        if (st_eq(&nh, &head)) break;
        head = nh;
    }
    *s = exitst;
}

/* leaving a block (at its end, or by Break/Continue): its owners free their objects */
static void leave_block(Stmt *st, State *s, Pos endp) {
    for (int k = st->wvars.len - 1; k >= 0; k--) { /* freed in reverse order of declaration */
        Var *v = st->wvars.items[k];
        int i = slot(v);
        if (i < 0 || !s->reach) continue;
        if (carrier(s, i) && (s->bor[i] & B_DANGLING) && (s->own[i] & (O_OWNED | O_UNK)))
            berror(endp, "'%s' still refers to another object through a 'var' field when it is freed at the end of its block, but %s",
                   v->name, s->why[i] ? s->why[i] : "that object is gone");
        if (tv.items[i].kind == TV_OWNER && (s->own[i] & (O_OWNED | O_UNK)))
            invalidate(s, sfmt("#%d", v->id), sfmt("'%s' freed its object at the end of its block (line %d)", v->name, endp.line));
        if (v->is_stack) invalidate(s, sfmt("#%d", v->id), sfmt("'%s' went out of scope at the end of its block (line %d)", v->name, endp.line));
        s->own[i] = O_NULL;
        s->nn[i] = N_UNINIT;
    }
}

static void an_stmt(Stmt *st, State *s) {
    if (!s->reach) return;
    switch (st->kind) {
    case S_EMPTY: case S_FORIN: return; /* for-in loops are rewritten by sema */
    case S_ASSIGN:
        if (st->bind == 3) do_heap_to_stack(st->a->var, st->b, s, st->pos);
        else if (st->bind == 2) do_stack_move(st->a->var, st->b->var, s, st->pos);
        else if (st->bind) do_bind(st->a->var, st->b, s, st->pos);
        else do_assign(st->a, st->b, s, st->pos);
        return;
    case S_CALL: {
        Expr *e = st->a;
        if (e->kind == E_RCALL) {
            eval_src(e, s); /* an owned result nobody takes is freed by the generated code */
        } else if (e->kind == E_BUILTIN) {
            builtin_effects(e, s);
        } else
            use_expr(e, s);
        return;
    }
    case S_COMPOUND: an_list(&st->body, s); return;
    case S_IF: {
        use_expr(st->a, s);
        State t = st_copy(s);
        refine(st->a, 1, &t);
        refine(st->a, 0, s);
        an_stmt(st->s1, &t);
        if (st->s2) an_stmt(st->s2, s);
        st_join(s, &t);
        {
            VarList ow = {0};
            cond_owners(st->a, &ow);
            for (int i = 0; i < ow.len; i++)
                invalidate_root(s, ow.items[i], sfmt("'%s' was freed at the end of the 'if' at line %d", ow.items[i]->name, st->pos.line));
        }
        return;
    }
    case S_WHILE: loop_body(st, s, 0); return;
    case S_REPEAT: loop_body(st, s, 1); return;
    case S_FOR:
        use_expr(st->a, s);
        use_expr(st->b, s);
        loop_body(st, s, 2);
        return;
    case S_CASE: {
        use_expr(st->a, s);
        State res = st_unreach();
        for (int i = 0; i < st->arms.len; i++) {
            CaseArm *arm = &st->arms.items[i];
            for (int k = 0; k < arm->lo.len; k++) {
                use_expr(arm->lo.items[k], s);
                use_expr(arm->hi.items[k], s);
            }
            State t = st_copy(s);
            an_stmt(arm->body, &t);
            st_join(&res, &t);
        }
        if (st->has_else) {
            State t = st_copy(s);
            an_list(&st->else_, &t);
            st_join(&res, &t);
        } else
            st_join(&res, s);
        *s = res;
        return;
    }
    case S_BREAK: case S_CONTINUE:
        if (loops.len) {
            LoopCtx *lc = loops.items[loops.len - 1];
            State out = st_copy(s); /* the blocks inside the loop end here: their owners free */
            for (int k = cblocks.len - 1; k >= lc->blk; k--) leave_block(cblocks.items[k], &out, st->pos);
            st_join(st->kind == S_BREAK ? &lc->brk : &lc->cont, &out);
        }
        s->reach = 0;
        return;
    case S_BLOCK: {
        /* entering the block: its variables start fresh */
        for (int k = 0; k < st->wvars.len; k++) {
            Var *v = st->wvars.items[k];
            int i = slot(v);
            if (i < 0) continue;
            s->own[i] = O_NULL;
            s->bor[i] = B_VALID;
            s->org[i].len = 0;
            s->why[i] = NULL;
            s->nn[i] = N_UNINIT;
            if (v->is_stack) origin_add(&s->org[i], sfmt("#%d", v->id), v->name, v, 1);
        }
        vpush(cblocks, st);
        an_list(&st->blk->body, s);
        cblocks.len--;
        leave_block(st, s, st->blk->body.len ? st->blk->body.items[st->blk->body.len - 1]->pos : st->pos);
        return;
    }
    case S_EXIT:
        if (st->a && unit && unit->result) {
            Expr *rv = new_expr(E_VAR, st->pos);
            rv->var = unit->result;
            rv->type = unit->result->type;
            do_assign(rv, st->a, s, st->pos);
        }
        exit_checks(s, st->pos);
        s->reach = 0;
        return;
    }
}

/* ============================================================== write sets */

static void vs_add(VarSet *w, Var *v) {
    for (int i = 0; i < w->len; i++)
        if (w->items[i] == v) return;
    vpush(*w, v);
}

static int nonlocal_to(Var *v, Routine *r) {
    if (v->vkind == VK_SELF) return 0;
    if (v->is_global) return 1;
    return v->level < r->level;
}

static VarSet *cur_ws, *cur_rs;
static Routine *cur_wr;
static void ws_target(Expr *e) {
    Var *root = NULL;
    if (!path_of(e, &root) || !root) return;
    if (nonlocal_to(root, cur_wr)) vs_add(cur_ws, root);
}

static void ws_expr(Expr *e);
static void ws_stmt(Stmt *s);

static void ws_expr(Expr *e) {
    if (!e) return;
    switch (e->kind) {
    case E_VAR:
        if (nonlocal_to(e->var, cur_wr) && is_ref_type(e->var->type) && !NILABLE(e->var->type)) vs_add(cur_rs, e->var);
        return;
    case E_RCALL:
        ws_expr(e->a);
        if (e->a && is_free_call(e)) ws_target(e->a);
        for (int i = 0; i < e->args.len; i++) {
            Param *pm = &e->rt->params.items[i];
            if (pm->mode == PM_VAR || pm->mode == PM_OUT || pm->mode == PM_OWNED) ws_target(e->args.items[i]);
            ws_expr(e->args.items[i]);
        }
        return;
    case E_BUILTIN:
        if (e->bi == BI_NEW || e->bi == BI_DISPOSE || e->bi == BI_FREEANDNIL || e->bi == BI_TAKE || e->bi == BI_BOXFREE)
            if (e->args.len && e->args.items[0]->kind != E_TYPEREF) ws_target(e->args.items[0]);
        for (int i = 0; i < e->args.len; i++) ws_expr(e->args.items[i]);
        ws_expr(e->b);
        return;
    case E_FMT: ws_expr(e->a); ws_expr(e->b); ws_expr(e->c); return;
    case E_IN:
        ws_expr(e->a);
        for (int i = 0; i < e->b->args.len; i++) {
            ws_expr(e->b->args.items[i]);
            ws_expr(e->b->args2.items[i]);
        }
        return;
    default:
        ws_expr(e->a);
        ws_expr(e->b);
        ws_expr(e->c);
        for (int i = 0; i < e->args.len; i++) ws_expr(e->args.items[i]);
        return;
    }
}

static void ws_stmt(Stmt *s) {
    switch (s->kind) {
    case S_ASSIGN:
        if (is_owning_type(s->a->type)) ws_target(s->a);
        if (s->b->kind == E_VAR && is_owning_type(s->b->type) && nonlocal_to(s->b->var, cur_wr)) vs_add(cur_ws, s->b->var);
        if (s->a->kind != E_VAR) ws_expr(s->a);
        ws_expr(s->b);
        return;
    case S_CALL: ws_expr(s->a); return;
    case S_COMPOUND: case S_REPEAT:
        for (int i = 0; i < s->body.len; i++) ws_stmt(s->body.items[i]);
        ws_expr(s->a);
        return;
    case S_IF: case S_WHILE:
        ws_expr(s->a);
        ws_stmt(s->s1);
        if (s->s2) ws_stmt(s->s2);
        return;
    case S_FOR:
        ws_expr(s->a);
        ws_expr(s->b);
        ws_stmt(s->s1);
        return;
    case S_CASE:
        ws_expr(s->a);
        for (int i = 0; i < s->arms.len; i++) ws_stmt(s->arms.items[i].body);
        for (int i = 0; i < s->else_.len; i++) ws_stmt(s->else_.items[i]);
        return;
    case S_EXIT: ws_expr(s->a); return;
    case S_BLOCK:
        for (int i = 0; i < s->blk->body.len; i++) ws_stmt(s->blk->body.items[i]);
        return;
    default: return;
    }
}

typedef VEC(Routine *) RtSet;
static RtSet *callees;
static void cl_expr(Expr *e, RtSet *c) {
    if (!e) return;
    if (e->kind == E_RCALL) {
        int dup = 0;
        for (int i = 0; i < c->len; i++)
            if (c->items[i] == e->rt) dup = 1;
        if (!dup) vpush(*c, e->rt);
    }
    if (e->kind == E_IN) {
        cl_expr(e->a, c);
        for (int i = 0; i < e->b->args.len; i++) {
            cl_expr(e->b->args.items[i], c);
            cl_expr(e->b->args2.items[i], c);
        }
        return;
    }
    cl_expr(e->a, c);
    cl_expr(e->b, c);
    cl_expr(e->c, c);
    for (int i = 0; i < e->args.len; i++) cl_expr(e->args.items[i], c);
}
static void cl_stmt(Stmt *s, RtSet *c) {
    if (!s) return;
    cl_expr(s->a, c);
    cl_expr(s->b, c);
    cl_expr(s->c, c);
    cl_stmt(s->s1, c);
    cl_stmt(s->s2, c);
    for (int i = 0; i < s->body.len; i++) cl_stmt(s->body.items[i], c);
    for (int i = 0; i < s->arms.len; i++) {
        cl_stmt(s->arms.items[i].body, c);
        for (int k = 0; k < s->arms.items[i].lo.len; k++) cl_expr(s->arms.items[i].lo.items[k], c);
    }
    for (int i = 0; i < s->else_.len; i++) cl_stmt(s->else_.items[i], c);
    for (int i = 0; i < s->exprs.len; i++) cl_expr(s->exprs.items[i], c);
    if (s->kind == S_BLOCK)
        for (int i = 0; i < s->blk->body.len; i++) cl_stmt(s->blk->body.items[i], c);
}

static void compute_write_sets(void) {
    int maxid = 0;
    for (int i = 0; i < prog->routines.len; i++)
        if (prog->routines.items[i]->id > maxid) maxid = prog->routines.items[i]->id;
    wsets = xcalloc((size_t)maxid + 1, sizeof(VarSet));
    rsets = xcalloc((size_t)maxid + 1, sizeof(VarSet));
    callees = xcalloc((size_t)maxid + 1, sizeof(RtSet));
    for (int i = 0; i < prog->routines.len; i++) {
        Routine *r = prog->routines.items[i];
        cur_wr = r;
        cur_ws = &wsets[r->id];
        cur_rs = &rsets[r->id];
        for (int k = 0; k < r->body.len; k++) {
            ws_stmt(r->body.items[k]);
            cl_stmt(r->body.items[k], &callees[r->id]);
        }
    }
    /* transitive closure; virtual calls include all methods of the same name */
    /* a call through a function value may reach every routine that is used as a value */
    for (int i = 0; i < prog->routines.len; i++) {
        Routine *r = prog->routines.items[i];
        if (!r->is_sig) continue;
        for (int q = 0; q < prog->routines.len; q++)
            if (prog->routines.items[q]->addr_taken) vpush(callees[r->id], prog->routines.items[q]);
    }
    for (int changed = 1; changed;) {
        changed = 0;
        for (int i = 0; i < prog->routines.len; i++) {
            Routine *r = prog->routines.items[i];
            for (int k = 0; k < callees[r->id].len; k++) {
                Routine *c = callees[r->id].items[k];
                for (int q = 0; q < prog->routines.len; q++) {
                    Routine *cc = prog->routines.items[q];
                    if (cc != c && !(c->slot >= 0 && cc->owner && STREQ(cc->name, c->name))) continue;
                    for (int w = 0; w < 2; w++) {
                        VarSet *src = w ? &rsets[cc->id] : &wsets[cc->id];
                        VarSet *dst = w ? &rsets[r->id] : &wsets[r->id];
                        for (int j = 0; j < src->len; j++) {
                            Var *v = src->items[j];
                            if (!nonlocal_to(v, r) && !r->is_sig) continue; /* a function type stands for its closures too */
                            int before = dst->len;
                            vs_add(dst, v);
                            if (dst->len != before) changed = 1;
                        }
                    }
                }
            }
        }
    }
}

/* ============================================================== units */

static void track(Var *v, int kind, int varparam) {
    TVar t = {v, kind, varparam};
    vpush(tv, t);
    v->bslot = tv.len;
}

static void untrack_all(void) {
    for (int i = 0; i < tv.len; i++) tv.items[i].v->bslot = 0;
    tv.len = 0;
    loops.len = 0;
}

static void track_var(Var *v) {
    if (v->is_alias) { /* var x: Integer := y: a borrow of y */
        track(v, TV_BORROW, 0);
        return;
    }
    if (v->no_drop && has_owned(v->type) && !is_ref_type(v->type)) { /* 'is some x' on a value that owns objects: a view */
        track(v, TV_BORROW, 0);
        return;
    }
    if (!is_owning_type(v->type)) return;
    track(v, v->borrowed ? TV_BORROW : TV_OWNER, 0);
}

static void check_unit(Routine *r, StmtVec *body, Pos end) {
    State s = st_new();
    for (int i = 0; i < tv.len; i++) {
        Var *v = tv.items[i].v;
        if (v->vkind == VK_PARAM)
            s.nn[i] = NILABLE(v->type) ? (N_NIL | N_NN) : v->mode == PM_OUT ? N_UNINIT : N_NN;
        else
            s.nn[i] = N_UNINIT;
        if (v->vkind == VK_PARAM && tv.items[i].kind == TV_BORROW)
            origin_add(&s.org[i], sfmt("#%d", v->id), v->name, v, 0);
        if (v->is_stack) origin_add(&s.org[i], sfmt("#%d", v->id), v->name, v, !v->is_global);
        if (tv.items[i].kind == TV_OWNER) {
            if (tv.items[i].varparam) s.own[i] = O_UNK;
            else if (v->vkind == VK_PARAM) s.own[i] = O_OWNED; /* owned parameter */
            else s.own[i] = O_NULL;
        } else
            s.bor[i] = B_VALID;
    }
    for (int k = 0; k < NFD; k++) s.fd[k] = N_UNINIT;
    (void)r;
    an_list(body, &s);
    exit_checks(&s, end);
}

int borrow_check(Program *p) {
    prog = p;
    nerrors = 0;
    if (p->tobject)
        for (int i = 0; i < p->tobject->methods.len; i++)
            if (STREQ(p->tobject->methods.items[i]->name, "Free")) tobject_free = p->tobject->methods.items[i];
    { /* write sets for all routines, including the generic bodies with placeholder types */
        __typeof__(p->routines) saved = p->routines, all = {0};
        for (int i = 0; i < saved.len; i++) vpush(all, saved.items[i]);
        for (int i = 0; i < p->arch_routines.len; i++) vpush(all, p->arch_routines.items[i]);
        p->routines = all;
        compute_write_sets();
        p->routines = saved;
    }
    for (int i = 0; i < p->structs.len && g_nil_check; i++) {
        Struct *c = p->structs.items[i];
        if (c->kind != SK_CLASS) continue;
        Field *f = own_nonnil_field(c);
        int has_ctor = 0;
        for (int k = 0; k < c->methods.len; k++)
            if (c->methods.items[k]->kind == RK_CTOR) has_ctor = 1;
        if (f && !has_ctor)
            nerror(c->pos, "class '%s' has the non-nilable field '%s' but declares no constructor that initializes it", c->name,
                   f->name);
    }

    /* generic bodies with placeholder types first: a type parameter without Copy is treated like an object;
       a generic with errors is reported once, its instances are not checked again */
    int ngen = 0;
    for (int i = 0; i < p->arch_routines.len; i++)
        if (p->arch_routines.items[i]->gen_id > ngen) ngen = p->arch_routines.items[i]->gen_id;
    for (int i = 0; i < p->routines.len; i++)
        if (p->routines.items[i]->gen_id > ngen) ngen = p->routines.items[i]->gen_id;
    char *broken = xcalloc((size_t)ngen + 1, 1);
    for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < (pass ? p->routines.len : p->arch_routines.len); i++) {
        Routine *r = pass ? p->routines.items[i] : p->arch_routines.items[i];
        where_flags(r, !pass); /* the generic body of a 'where' method may use what its constraints allow */
        if (r->prelude || r->is_abstract || r->is_external || r->is_sig || !r->defined || r->not_generated) continue;
        if (pass && r->gen_id && broken[r->gen_id]) continue;
        g_param_owning = !pass;
        arch_note = pass ? "" : sfmt(" (in the generic %s%s%s: a type parameter without 'Copy' may be an object)",
                                    r->owner ? r->owner->name : "", r->owner ? "." : "", r->name);
        int before = nerrors;
        unit = r;
        unit_name = r->owner ? sfmt("%s.%s", r->owner->name, r->name) : r->name;
        cfields.len = 0;
        cneed_inh = 0;
        if (r->kind == RK_CTOR && r->owner && g_nil_check) {
            Struct *c = r->owner;
            for (int k = 0; k < c->fields.len; k++) {
                Field *f = c->fields.items[k];
                if (f->owner == c && is_ref_type(f->type) && !NILABLE(f->type)) vpush(cfields, f);
            }
            for (Struct *x = c->parent; x; x = x->parent)
                if (own_nonnil_field(x)) cneed_inh = 1;
        }
        for (int k = 0; k < r->params.len; k++) {
            Param *pm = &r->params.items[k];
            if (!is_owning_type(pm->type) || !pm->var) continue;
            if (pm->mode == PM_OWNED) track(pm->var, TV_OWNER, 0);
            else if (pm->mode == PM_VAR || pm->mode == PM_OUT) track(pm->var, TV_OWNER, 1);
            else track(pm->var, TV_BORROW, 0);
        }
        if (r->result && is_owning_type(r->result->type)) track(r->result, r->ret_borrowed ? TV_BORROW : TV_OWNER, 0);
        for (int k = 0; k < r->locals.len; k++) track_var(r->locals.items[k]);
        Pos end = r->pos;
        if (r->body.len) end = r->body.items[r->body.len - 1]->pos;
        check_unit(r, &r->body, end);
        untrack_all();
        cfields.len = 0;
        cneed_inh = 0;
        if (!pass && nerrors > before && r->gen_id) broken[r->gen_id] = 1;
    }
    g_param_owning = 0;
    arch_note = "";
    unit = NULL;
    unit_name = "the program";
    for (int k = 0; k < p->globals.len; k++) track_var(p->globals.items[k]);
    StmtVec all = {0};
    for (int i = 0; i < p->init.len; i++) vpush(all, p->init.items[i]);
    for (int i = 0; i < p->body.len; i++) vpush(all, p->body.items[i]);
    check_unit(NULL, &all, p->end_pos);
    untrack_all();
    return nerrors;
}
