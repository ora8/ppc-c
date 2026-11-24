/* parser.c - lexer and recursive-descent parser (case-sensitive Pascal) */
#include "ast.h"

/* ================================================================ lexer */

typedef enum { TK_IDENT, TK_KW, TK_INT, TK_REAL, TK_STR, TK_SYM, TK_EOF } TokKind;

typedef struct {
    TokKind kind;
    const char *s; /* identifier / keyword / symbol text */
    long long ival;
    double rval;
    char *str;
    int slen;
    Pos pos;
} Token;

static const char *KEYWORDS[] = {
    "and", "array", "as", "begin", "case", "class", "const", "constructor", "destructor", "div",
    "do", "downto", "else", "end", "for", "function", "goto", "if", "implementation", "in",
    "inherited", "interface", "is", "label", "mod", "nil", "none", "some", "optional", "not", "object", "of", "or", "packed",
    "procedure", "program", "property", "record", "repeat", "set", "shl", "shr", "then", "to",
    "type", "unit", "until", "uses", "var", "while", "with", "xor", "try",
    "loop", "val", "vao", "default", "result", "ok", "error", "module", "import", NULL};

static const char *SYMS[] = {":=", "<=", ">=", "<>", "..", "+", "-", "*", "/", "=", "<", ">", "(",
                             ")", "[", "]", ",", ";", ":", ".", "^", "@", NULL};

typedef VEC(Token) TokVec;

static TokVec tokenize(const char *src) {
    TokVec out = {0};
    const char *b = src;
    size_t i = 0, n = strlen(src);
    int line = 1, col = 1;
#define ADV()                 \
    do {                      \
        if (b[i] == '\n') {   \
            line++;           \
            col = 1;          \
        } else                \
            col++;            \
        i++;                  \
    } while (0)
    while (i < n) {
        char c = b[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ADV();
            continue;
        }
        Pos pos = {line, col, g_filename};
        if (c == '{') {
            while (i < n && b[i] != '}') ADV();
            if (i >= n) error_at(pos, "unterminated comment");
            ADV();
            continue;
        }
        if (c == '(' && i + 1 < n && b[i + 1] == '*') {
            ADV();
            ADV();
            while (i + 1 < n && !(b[i] == '*' && b[i + 1] == ')')) ADV();
            if (i + 1 >= n) error_at(pos, "unterminated comment");
            ADV();
            ADV();
            continue;
        }
        if (c == '/' && i + 1 < n && b[i + 1] == '/') {
            while (i < n && b[i] != '\n') ADV();
            continue;
        }
        Token t = {0};
        t.pos = pos;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            size_t st = i;
            while (i < n && ((b[i] >= 'a' && b[i] <= 'z') || (b[i] >= 'A' && b[i] <= 'Z') ||
                             (b[i] >= '0' && b[i] <= '9') || b[i] == '_'))
                ADV();
            char *w = xstrndup(b + st, i - st);
            t.kind = TK_IDENT;
            t.s = w;
            for (int k = 0; KEYWORDS[k]; k++)
                if (STREQ(KEYWORDS[k], w)) {
                    t.kind = TK_KW;
                    t.s = KEYWORDS[k];
                }
            vpush(out, t);
            continue;
        }
        if (c >= '0' && c <= '9') {
            size_t st = i;
            int is_real = 0;
            while (i < n && b[i] >= '0' && b[i] <= '9') ADV();
            if (i + 1 < n && b[i] == '.' && b[i + 1] >= '0' && b[i + 1] <= '9') {
                is_real = 1;
                ADV();
                while (i < n && b[i] >= '0' && b[i] <= '9') ADV();
            }
            if (i < n && (b[i] == 'e' || b[i] == 'E')) {
                size_t si = i;
                int sl = line, sc = col;
                ADV();
                if (i < n && (b[i] == '+' || b[i] == '-')) ADV();
                if (i < n && b[i] >= '0' && b[i] <= '9') {
                    is_real = 1;
                    while (i < n && b[i] >= '0' && b[i] <= '9') ADV();
                } else {
                    i = si;
                    line = sl;
                    col = sc;
                }
            }
            char *txt = xstrndup(b + st, i - st);
            if (is_real) {
                t.kind = TK_REAL;
                t.rval = strtod(txt, NULL);
            } else {
                t.kind = TK_INT;
                t.ival = strtoll(txt, NULL, 10);
            }
            vpush(out, t);
            continue;
        }
        if (c == '$') {
            ADV();
            size_t st = i;
            while (i < n && ((b[i] >= '0' && b[i] <= '9') || (b[i] >= 'a' && b[i] <= 'f') ||
                             (b[i] >= 'A' && b[i] <= 'F')))
                ADV();
            if (st == i) error_at(pos, "bad hex literal");
            t.kind = TK_INT;
            t.ival = strtoll(xstrndup(b + st, i - st), NULL, 16);
            vpush(out, t);
            continue;
        }
        if (c == '\'' || c == '#') {
            Buf s = {0};
            for (;;) {
                if (i < n && b[i] == '\'') {
                    ADV();
                    for (;;) {
                        if (i >= n || b[i] == '\n') error_at(pos, "unterminated string");
                        if (b[i] == '\'') {
                            if (i + 1 < n && b[i + 1] == '\'') {
                                buf_putn(&s, "'", 1);
                                ADV();
                                ADV();
                                continue;
                            }
                            ADV();
                            break;
                        }
                        buf_putn(&s, b + i, 1);
                        ADV();
                    }
                } else if (i < n && b[i] == '#') {
                    ADV();
                    long v = -1;
                    size_t st = i;
                    if (i < n && b[i] == '$') {
                        ADV();
                        st = i;
                        while (i < n && ((b[i] >= '0' && b[i] <= '9') || (b[i] >= 'a' && b[i] <= 'f') ||
                                         (b[i] >= 'A' && b[i] <= 'F')))
                            ADV();
                        if (i > st) v = strtol(xstrndup(b + st, i - st), NULL, 16);
                    } else {
                        while (i < n && b[i] >= '0' && b[i] <= '9') ADV();
                        if (i > st) v = strtol(xstrndup(b + st, i - st), NULL, 10);
                    }
                    if (v < 0 || v > 255) error_at(pos, "bad character code");
                    char ch = (char)v;
                    buf_putn(&s, &ch, 1);
                } else
                    break;
            }
            t.kind = TK_STR;
            t.slen = (int)s.len;
            t.str = s.d ? s.d : xstrdup("");
            vpush(out, t);
            continue;
        }
        int matched = 0;
        for (int k = 0; SYMS[k]; k++) {
            size_t l = strlen(SYMS[k]);
            if (i + l <= n && strncmp(b + i, SYMS[k], l) == 0) {
                t.kind = TK_SYM;
                t.s = SYMS[k];
                for (size_t q = 0; q < l; q++) ADV();
                vpush(out, t);
                matched = 1;
                break;
            }
        }
        if (!matched) error_at(pos, "unexpected character '%c'", c);
    }
    Token e = {0};
    e.kind = TK_EOF;
    e.pos = (Pos){line, col, g_filename};
    vpush(out, e);
    return out;
#undef ADV
}

/* ================================================================ parser */

static TokVec T;
static int P;
static TokVec *cur_toks; /* the token array T belongs to (kept for parsing generics again) */

#define CUR (T.items[P])
static Pos pos(void) { return CUR.pos; }
static void next(void) {
    if (P < T.len - 1) P++;
}
static int is_kw(const char *k) { return CUR.kind == TK_KW && STREQ(CUR.s, k); }
static int is_sym(const char *s) { return CUR.kind == TK_SYM && STREQ(CUR.s, s); }
static int is_ident(const char *s) { return CUR.kind == TK_IDENT && STREQ(CUR.s, s); }
static int peek_is_ident(int k) {
    int i = P + k < T.len ? P + k : T.len - 1;
    return T.items[i].kind == TK_IDENT;
}
static const char *describe(void) {
    switch (CUR.kind) {
    case TK_IDENT: return sfmt("identifier '%s'", CUR.s);
    case TK_KW: return sfmt("'%s'", CUR.s);
    case TK_INT: return sfmt("number %lld", CUR.ival);
    case TK_REAL: return "real number";
    case TK_STR: return "string literal";
    case TK_SYM: return sfmt("'%s'", CUR.s);
    default: return "end of file";
    }
}
static int accept_kw(const char *k) {
    if (is_kw(k)) {
        next();
        return 1;
    }
    return 0;
}
static int accept_sym(const char *s) {
    if (is_sym(s)) {
        next();
        return 1;
    }
    return 0;
}
static void expect_kw(const char *k) {
    if (!accept_kw(k)) error_at(pos(), "'%s' expected but %s found", k, describe());
}
static void expect_sym(const char *s) {
    if (!accept_sym(s)) error_at(pos(), "'%s' expected but %s found", s, describe());
}
static Ident ident(void) {
    if (CUR.kind != TK_IDENT) {
        if (CUR.kind == TK_KW)
            error_at(pos(), "identifier expected but keyword '%s' found", CUR.s);
        error_at(pos(), "identifier expected but %s found", describe());
    }
    Ident id = {xstrdup(CUR.s), CUR.pos};
    next();
    return id;
}
static IdentVec ident_list(void) {
    IdentVec v = {0};
    vpush(v, ident());
    while (accept_sym(",")) vpush(v, ident());
    return v;
}

Expr *new_expr(ExprKind k, Pos p) {
    Expr *e = NEW(Expr);
    e->kind = k;
    e->pos = p;
    return e;
}
Stmt *new_stmt(StmtKind k, Pos p) {
    Stmt *s = NEW(Stmt);
    s->kind = k;
    s->pos = p;
    return s;
}

static Expr *parse_expr(void);
static Expr *parse_simple(void);
static Expr *parse_postfix(void);
static Stmt *parse_stmt(void);
static TypeExpr *parse_type(void);
static TypeExpr *parse_type_core(void);
static TypeExprVec parse_targs(void);
static void parse_tparams(IdentVec *names, ConsVec *cons);
static Block *parse_block(void);
static void parse_decls(Block *blk);

/* ------------------------------------------------------------- types */

static TypeExpr *new_te(TEKind k, Pos p) {
    TypeExpr *t = NEW(TypeExpr);
    t->kind = k;
    t->pos = p;
    return t;
}

/* 'var T' in a field or function result: a reference that does not own its object */
static int accept_borrowed(void) {
    if (accept_kw("var")) return 1;
    return 0;
}

static const char *DIRECTIVES[] = {"virtual", "override", "abstract", "overload", "reintroduce",
                                   "static", "inline", "cdecl", "forward", "register", "auto", NULL};

static int is_directive(void) {
    if (CUR.kind != TK_IDENT) return 0;
    for (int k = 0; DIRECTIVES[k]; k++)
        if (STREQ(CUR.s, DIRECTIVES[k])) return 1;
    return 0;
}

/* directives after a routine header: virtual; override; abstract; forward; const; */
static void parse_directives(int *virt, int *abs, int *fwd, int *cnst, int *autog) {
    for (;;) {
        if (is_kw("const") && T.items[P + 1].kind == TK_SYM && STREQ(T.items[P + 1].s, ";")) {
            next(); /* const;  -- method does not change Self / routine has no side effects */
            next();
            *cnst = 1;
            continue;
        }
        if (!is_directive()) break;
        const char *d = CUR.s;
        next();
        if (STREQ(d, "virtual")) *virt = VIRT_VIRTUAL;
        else if (STREQ(d, "override")) *virt = VIRT_OVERRIDE;
        else if (STREQ(d, "abstract")) *abs = 1;
        else if (STREQ(d, "forward")) *fwd = 1;
        else if (STREQ(d, "auto")) {
            if (!autog) error_at(pos(), "'auto' belongs to the declaration in the class or record ('function Clone: T; const; auto;')");
            *autog = 1;
        }
        expect_sym(";");
    }
}

static ParamGroupVec parse_params(void) {
    ParamGroupVec v = {0};
    if (!accept_sym("(")) return v;
    if (accept_sym(")")) return v;
    for (;;) {
        ParamGroup g = {0};
        if (accept_kw("var")) g.mode = PM_VAR;
        else if (accept_kw("vao")) g.mode = PM_OWNED; /* takes over the ownership */
        else if (is_kw("const") || is_ident("constref"))
            error_at(pos(), "read-only parameters are written 'x: T const' (passed by reference, not copied)");
        else if (is_ident("owned"))
            error_at(pos(), "'owned' was replaced by 'vao' (for example 'procedure Add(vao Item: TItem)')");
        else if (is_ident("out")) {
            next();
            g.mode = PM_OUT;
        } else
            g.mode = PM_VALUE;
        g.names = ident_list();
        if (!accept_sym(":")) error_at(pos(), "untyped parameters are not supported");
        if (is_kw("array") && !(T.items[P + 1].kind == TK_KW && STREQ(T.items[P + 1].s, "of")))
            error_at(pos(), "fixed-size array types in parameter lists must be named (declare a type), or use 'array of T'");
        g.ty = parse_type();
        if (is_sym("=")) error_at(pos(), "default parameter values are not supported");
        vpush(v, g);
        if (!accept_sym(";")) break;
    }
    expect_sym(")");
    return v;
}

/* old prefix form 'const function' / 'const procedure' (now written as a trailing 'const;' directive) */
static int const_routine_ahead(void) {
    return is_kw("const") && T.items[P + 1].kind == TK_KW &&
           (STREQ(T.items[P + 1].s, "function") || STREQ(T.items[P + 1].s, "procedure") ||
            STREQ(T.items[P + 1].s, "constructor") || STREQ(T.items[P + 1].s, "destructor"));
}

static int last_const; /* the routine was declared with 'const' */
static int class_routine_ahead(void) {
    return is_kw("class") && T.items[P + 1].kind == TK_KW &&
           (STREQ(T.items[P + 1].s, "function") || STREQ(T.items[P + 1].s, "procedure"));
}

static int last_classm; /* 'class function' / 'class procedure': no Self, called on the class */
static int routine_kind(void) {
    int k = RK_PROC;
    last_const = 0;
    last_classm = 0;
    if (class_routine_ahead()) {
        next();
        last_classm = 1;
    }
    if (const_routine_ahead())
        error_at(pos(), "write 'const' at the end of the declaration: '%s Name(...); const;'", T.items[P + 1].s);
    if (is_kw("function")) k = RK_FUNC;
    else if (is_kw("constructor")) k = RK_CTOR;
    else if (is_kw("destructor")) k = RK_DTOR;
    next();
    return k;
}

static int is_routine_kw(void) {
    return is_kw("procedure") || is_kw("function") || is_kw("constructor") || is_kw("destructor") ||
           const_routine_ahead() || class_routine_ahead();
}

static Expr *parse_postfix(void);

/* 'TFoo.Create(args)' in place of a type: the variable is created at its declaration */
static int ctor_decl_ahead(void) {
    return CUR.kind == TK_IDENT && T.items[P + 1].kind == TK_SYM && STREQ(T.items[P + 1].s, ".") &&
           T.items[P + 2].kind == TK_IDENT;
}

static void parse_decl_type(VarDecl *v) {
    if (ctor_decl_ahead())
        error_at(pos(), "objects are created in the initializer: 'vao x := %s.%s(...)'", CUR.s, T.items[P + 2].s);
    if (0) {
        Pos tp = pos();
        Expr *c = parse_postfix();
        Expr *m = c->kind == E_CALL ? c->a : c;
        if (m->kind != E_MEMBER || m->a->kind != E_IDENT) error_at(tp, "'Type.Constructor(...)' expected");
        TypeExpr *t = NEW(TypeExpr);
        t->kind = TE_NAMED;
        t->pos = tp;
        t->name = m->a->id;
        v->ty = t;
        v->ctor = c;
        return;
    }
    v->ty = parse_type();
}

static StructDecl *parse_struct(int kind) {
    StructDecl *sd = NEW(StructDecl);
    sd->kind = kind;
    if (kind == SK_CLASS && accept_sym("(")) {
        sd->parent = ident();
        sd->has_parent = 1;
        if (is_sym("<")) sd->parent_targs = parse_targs();
        if (is_sym(",")) error_at(pos(), "interfaces are not supported");
        expect_sym(")");
    }
    for (;;) {
        if (accept_kw("end")) break;
        if (is_ident("private") || is_ident("protected") || is_ident("public") ||
            is_ident("published") || is_ident("strict")) {
            next();
            continue;
        }
        if (accept_kw("var") || accept_kw("val")) continue;
        if (is_kw("class") && !(T.items[P + 1].kind == TK_KW && (STREQ(T.items[P + 1].s, "function") ||
                                                              STREQ(T.items[P + 1].s, "procedure"))))
            error_at(pos(), "class fields are not supported (only 'class function' / 'class procedure')");
        if (kind == SK_RECORD && is_kw("case")) error_at(pos(), "variant records are not supported");
        if (is_routine_kw()) {
            MethodDecl m = {0};
            m.kind = routine_kind();
            m.is_classm = last_classm;
            m.name = ident();
            m.params = parse_params();
            if (m.kind == RK_FUNC) {
                expect_sym(":");
                m.ret_borrowed = accept_borrowed();
                m.ret = parse_type();
            }
            expect_sym(";");
            int fwd = 0;
            parse_directives(&m.virt, &m.is_abstract, &fwd, &m.is_const, &m.is_auto);
            vpush(sd->methods, m);
            continue;
        }
        if (accept_kw("property")) {
            PropDecl pd = {0};
            pd.name = ident();
            expect_sym(":");
            pd.ty = parse_type();
            for (;;) {
                if (is_ident("read")) {
                    next();
                    pd.read = ident();
                    pd.has_read = 1;
                } else if (is_ident("write")) {
                    next();
                    pd.write = ident();
                    pd.has_write = 1;
                } else
                    break;
            }
            expect_sym(";");
            vpush(sd->props, pd);
            continue;
        }
        if (CUR.kind == TK_IDENT) {
            FieldDecl f = {0};
            f.names = ident_list();
            expect_sym(":");
            f.borrowed = accept_borrowed();
            f.ty = parse_type();
            if (!(kind == SK_RECORD && is_kw("end"))) expect_sym(";"); /* records: last ';' optional */
            vpush(sd->fields, f);
            continue;
        }
        error_at(pos(), "unexpected %s in %s declaration", describe(), kind == SK_RECORD ? "record" : "class");
    }
    return sd;
}

static TypeExpr *parse_type(void) {
    Pos p = pos();
    if (accept_kw("result")) { /* result T [of E]: a value or an error (like Result<T, E> in Rust) */
        TypeExpr *t = NEW(TypeExpr);
        t->kind = TE_RESULT;
        t->pos = p;
        if (!is_kw("of") && !(CUR.kind == TK_SYM && (STREQ(CUR.s, ";") || STREQ(CUR.s, ")") || STREQ(CUR.s, ":="))) &&
            !is_kw("const"))
            t->elem = parse_type();
        if (accept_kw("of")) t->errt = parse_type();
        if (is_kw("const") && T.items[P + 1].kind != TK_IDENT) {
            next();
            t->readonly = 1;
        }
        return t;
    }
    if (accept_kw("optional")) {
        TypeExpr *t = parse_type();
        if (t->nilable) error_at(p, "optional of optional is not allowed");
        t->nilable = 1;
        return t;
    }
    if (is_kw("const")) error_at(p, "write the modifier after the type: 'T const'");
    TypeExpr *t = parse_type_core();
    /* 'T const': read-only (values: unchangeable; objects: only readable through this reference) */
    if (is_kw("const") && T.items[P + 1].kind != TK_IDENT) {
        next();
        t->readonly = 1;
    }
    return t;
}

static int generic_args_ahead(void);
/* <T>, <A, B>, <T: Comparable>, <K: Comparable; V>, <T: Clone, Comparable> */
static void parse_tparams(IdentVec *names, ConsVec *cons) {
    expect_sym("<");
    for (;;) {
        IdentVec group = ident_list();
        IdentVec c = {0};
        if (accept_sym(":"))
            do {
                if (is_kw("class")) {
                    Ident k = {xstrdup("class"), pos()};
                    next();
                    vpush(c, k);
                } else
                    vpush(c, ident());
            } while (accept_sym(","));
        for (int i = 0; i < group.len; i++) {
            vpush(*names, group.items[i]);
            vpush(*cons, c);
        }
        if (!accept_sym(";")) break;
    }
    expect_sym(">");
}

static TypeExprVec parse_targs(void) {
    TypeExprVec v = {0};
    expect_sym("<");
    vpush(v, parse_type());
    while (accept_sym(",")) vpush(v, parse_type());
    expect_sym(">");
    return v;
}

static TypeExpr *parse_type_core(void) {
    Pos p = pos();
    if (CUR.kind == TK_IDENT && T.items[P + 1].kind == TK_SYM && STREQ(T.items[P + 1].s, "<")) {
        TypeExpr *t = new_te(TE_NAMED, p); /* generic instance: TListe<TKunde> */
        t->name = ident();
        t->targs = parse_targs();
        return t;
    }
    if (is_sym("^"))
        error_at(p, "pointer types are not supported: use a class (objects are owned by 'vao' variables, referenced by 'var' variables)");
    accept_kw("packed");
    if (accept_kw("array")) {
        TypeExpr *t = new_te(TE_ARRAY, p);
        if (is_kw("of")) { /* array of T: dynamic array */
            next();
            t->dyn = 1;
            t->elem = parse_type();
            return t;
        }
        expect_sym("[");
        vpush(t->ranges, parse_type());
        while (accept_sym(",")) vpush(t->ranges, parse_type());
        expect_sym("]");
        expect_kw("of");
        t->elem = parse_type();
        return t;
    }
    if (accept_kw("record")) { /* records: value types; may have methods and properties */
        TypeExpr *t = new_te(TE_RECORD, p);
        t->sd = parse_struct(SK_RECORD);
        t->fields = t->sd->fields;
        return t;
    }
    if (is_kw("object"))
        error_at(p, "'object' types are not supported: use 'class' (objects are created with 'vao x := TFoo.Create(...)')");
    if (accept_kw("class")) {
        if (is_sym(";")) return new_te(TE_CLASSFWD, p);
        if (is_kw("of")) error_at(pos(), "class references ('class of') are not supported");
        TypeExpr *t = new_te(TE_STRUCT, p);
        t->sd = parse_struct(SK_CLASS);
        return t;
    }
    if (is_kw("set")) error_at(pos(), "set types are not supported (set literals in 'in' tests are)");
    if (accept_sym("(")) {
        TypeExpr *t = new_te(TE_ENUM, p);
        t->enums = ident_list();
        expect_sym(")");
        return t;
    }
    if (is_ident("String")) {
        next();
        if (accept_sym("[")) {
            parse_expr();
            expect_sym("]");
        }
        return new_te(TE_STR, p);
    }
    Expr *e = parse_simple();
    if (accept_sym("..")) {
        TypeExpr *t = new_te(TE_SUBRANGE, p);
        t->lo = e;
        t->hi = parse_simple();
        return t;
    }
    if (e->kind != E_IDENT) error_at(p, "type expected");
    TypeExpr *t = new_te(TE_NAMED, p);
    t->name = e->id;
    return t;
}

/* ------------------------------------------------------------- declarations */

static Expr *parse_init(void) { return parse_expr(); }

static int in_interface; /* parsing a module's interface section: routine headers only */
static RoutineDecl *parse_routine(void) {
    RoutineDecl *d = NEW(RoutineDecl);
    d->toks = cur_toks;
    d->tok_start = P;
    d->pos = pos();
    d->kind = routine_kind();
    d->is_classm = last_classm;
    d->name = ident();
    if (is_sym("<")) { /* method of a generic type: procedure TListe<T>.Add */
        ConsVec cons = {0};
        parse_tparams(&d->cls_tparams, &cons);
        if (!is_sym(".")) { /* generic routine: function Max<T: Comparable>(a, b: T): T */
            d->tparams = d->cls_tparams;
            d->tcons = cons;
            d->cls_tparams = (IdentVec){0};
        } else
            for (int i = 0; i < cons.len; i++)
                if (cons.items[i].len)
                    error_at(cons.items[i].items[0].pos, "constraints belong to the declaration of the type, not to its methods: 'procedure %s<...>.%s'",
                             d->name.name, T.items[P + 1].s);
    }
    if (accept_sym(".")) {
        d->cls = d->name;
        d->has_cls = 1;
        d->name = ident();
    }
    if (is_sym("(")) {
        d->has_params = 1;
        d->params = parse_params();
    }
    if (d->kind == RK_FUNC && accept_sym(":")) {
        d->ret_borrowed = accept_borrowed();
        d->ret = parse_type();
    }
    expect_sym(";");
    int virt = 0, abs = 0;
    parse_directives(&virt, &abs, &d->forward, &d->is_const, NULL);
    if (in_interface) { /* module interface: only the header, the body is in the implementation */
        if (d->has_cls)
            error_at(d->pos, "method bodies belong to the implementation section; the interface only declares '%s' in its class", d->name.name);
        if (d->forward) error_at(d->pos, "routines in the interface need no 'forward'");
        if (is_kw("begin")) error_at(pos(), "the body of '%s' belongs to the implementation section", d->name.name);
        d->forward = 1;
        d->iface = 1;
        return d;
    }
    if (d->forward) return d;
    if (is_ident("external")) error_at(pos(), "external routines are not supported");
    d->body = parse_block();
    expect_sym(";");
    return d;
}

static int one_decl; /* 'while val i := 0; ...': exactly one declaration */
/* one declaration section (or routine); 0 when the declarations are over */
static int parse_decl1(Block *blk) {
    {
        if (one_decl && blk->decls.len) return 0;
        if (!const_routine_ahead() && accept_kw("const")) {
            Decl *d = NEW(Decl);
            d->kind = D_CONST;
            while (CUR.kind == TK_IDENT) {
                ConstDecl c = {0};
                c.name = ident();
                if (accept_sym(":")) c.ty = parse_type();
                expect_sym("=");
                c.value = c.ty ? parse_init() : parse_expr();
                expect_sym(";");
                vpush(d->consts, c);
            }
            vpush(blk->decls, d);
        } else if (accept_kw("type")) {
            Decl *d = NEW(Decl);
            d->kind = D_TYPE;
            while (CUR.kind == TK_IDENT) {
                TypeDecl t = {0};
                t.name = ident();
                if (is_sym("<")) parse_tparams(&t.tparams, &t.tcons); /* generic type: TListe<T> = class ... end */
                expect_sym("=");
                t.toks = cur_toks;
                t.tok_start = P;
                t.ty = parse_type();
                expect_sym(";");
                vpush(d->types, t);
            }
            vpush(blk->decls, d);
        } else if (is_kw("val") || is_kw("var") || is_kw("vao")) {
            /* val: values;  var: references that do not own (no freeing);  vao: owning references (freed at scope end) */
            const char *kw = CUR.s;
            next();
            Decl *d = NEW(Decl);
            d->kind = D_VAR;
            while (CUR.kind == TK_IDENT) {
                VarDecl v = {0};
                v.names = ident_list();
                if (accept_sym(":")) { /* without ': Type' the initializer decides the type */
                    parse_decl_type(&v);
                }
                v.borrowed = STREQ(kw, "var");
                v.is_heap = STREQ(kw, "vao");
                if (is_sym("=")) error_at(pos(), "variables are initialized with ':=' (for example '%s I := 5')", kw);
                if (accept_sym(":=")) v.init = parse_init();
                expect_sym(";");
                vpush(d->vars, v);
                if (one_decl) break;
            }
            if (!d->vars.len) error_at(pos(), "identifier expected in %s section", kw);
            vpush(blk->decls, d);
        } else if (is_kw("label")) {
            error_at(pos(), "labels/goto are not supported");
        } else if (is_routine_kw()) {
            Decl *d = NEW(Decl);
            d->kind = D_ROUTINE;
            d->rt = parse_routine();
            vpush(blk->decls, d);
        } else
            return 0;
    }
    return 1;
}

static int nest_open(void) {
    return is_kw("begin") || is_kw("case") || is_kw("record") || is_kw("repeat") || is_kw("class") || is_kw("try");
}

/* after a syntax error in a declaration: skip to its ';' (a routine together with its body) */
static void sync_decl(int routine) {
    int depth = 0;
    while (CUR.kind != TK_EOF) {
        if (nest_open() && !(is_kw("class") && T.items[P + 1].kind == TK_KW &&
                             (STREQ(T.items[P + 1].s, "function") || STREQ(T.items[P + 1].s, "procedure"))))
            depth++;
        else if (is_kw("end") || is_kw("until")) {
            if (depth > 0) depth--;
            if (depth == 0 && routine) { /* end of the routine body */
                next();
                accept_sym(";");
                return;
            }
        } else if (depth == 0 && is_sym(";")) {
            next();
            if (!routine || !(is_kw("val") || is_kw("var") || is_kw("vao") || is_kw("const") || is_kw("type") ||
                              is_kw("begin")))
                return;
            continue; /* the routine's own declarations and body follow */
        }
        next();
    }
}

static void parse_decls(Block *blk) {
    for (;;) {
        int routine = is_routine_kw();
        jmp_buf jb, *prev = g_recover;
        g_recover = &jb;
        if (setjmp(jb)) { /* syntax error: report it, skip the declaration, go on */
            g_recover = prev;
            sync_decl(routine);
            continue;
        }
        int more = parse_decl1(blk);
        g_recover = prev;
        if (!more) break;
    }
}

/* ------------------------------------------------------------- statements */

/* after a syntax error in a statement: skip to the next ';' or to the end of the statement list */
static void sync_stmt(void) {
    int depth = 0;
    while (CUR.kind != TK_EOF) {
        if (nest_open()) depth++;
        else if (is_kw("end") || is_kw("until")) {
            if (depth == 0) return;
            depth--;
        } else if (depth == 0 && is_sym(";"))
            return;
        next();
    }
}

static StmtVec parse_stmt_list(const char *term) {
    StmtVec *v = NEW(StmtVec); /* not a local: it must survive a longjmp */
    for (;;) {
        jmp_buf jb, *prev = g_recover;
        g_recover = &jb;
        if (setjmp(jb)) { /* syntax error: report it, skip the statement, go on */
            g_recover = prev;
            sync_stmt();
            if (accept_sym(";")) continue;
            break;
        }
        Stmt *s = parse_stmt();
        if (s->kind != S_EMPTY) vpush(*v, s);
        if (accept_sym(";")) {
            g_recover = prev;
            continue;
        }
        if (is_kw(term)) {
            g_recover = prev;
            break;
        }
        error_at(pos(), "';' expected but %s found", describe());
    }
    return *v;
}

static Stmt *parse_stmt(void) {
    Pos p = pos();
    if (is_sym(";") || is_kw("end") || is_kw("until") || is_kw("else")) return new_stmt(S_EMPTY, p);
    if (is_kw("val") || is_kw("var") || is_kw("vao") || is_kw("const")) {
        /* block statement: declarations followed by begin ... end form a new scope */
        Stmt *s = new_stmt(S_BLOCK, p);
        s->blk = NEW(Block);
        parse_decls(s->blk);
        expect_kw("begin");
        s->blk->body = parse_stmt_list("end");
        expect_kw("end");
        return s;
    }
    if (accept_kw("begin")) {
        Stmt *s = new_stmt(S_COMPOUND, p);
        s->body = parse_stmt_list("end");
        expect_kw("end");
        return s;
    }
    if (accept_kw("if")) {
        Stmt *s = new_stmt(S_IF, p);
        s->a = parse_expr();
        expect_kw("then");
        s->s1 = parse_stmt();
        if (accept_kw("else")) s->s2 = parse_stmt();
        return s;
    }
    if (accept_kw("while")) {
        if (is_kw("val") || is_kw("var") || is_kw("vao")) {
            /* while val i := 0; i < n do ...: i is declared once and lives as long as the loop */
            Stmt *b = new_stmt(S_BLOCK, p);
            b->blk = NEW(Block);
            one_decl = 1;
            parse_decls(b->blk);
            one_decl = 0;
            Stmt *s = new_stmt(S_WHILE, p);
            s->a = parse_expr();
            expect_kw("do");
            s->s1 = parse_stmt();
            vpush(b->blk->body, s);
            return b;
        }
        Stmt *s = new_stmt(S_WHILE, p);
        s->a = parse_expr();
        expect_kw("do");
        s->s1 = parse_stmt();
        return s;
    }
    if (accept_kw("loop")) { /* loop <statement>: endless loop, left with Break, Exit or try */
        Stmt *s = new_stmt(S_WHILE, p);
        s->a = NULL;
        s->s1 = parse_stmt();
        return s;
    }
    if (accept_kw("repeat")) {
        Stmt *s = new_stmt(S_REPEAT, p);
        s->body = parse_stmt_list("until");
        expect_kw("until");
        s->a = parse_expr();
        return s;
    }
    if (accept_kw("for")) {
        Stmt *s = new_stmt(S_FOR, p);
        s->decl_var = accept_kw("val"); /* for val i := 1 to n: the counter is declared by the loop */
        s->var = ident();
        expect_sym(":=");
        s->a = parse_expr();
        if (accept_kw("downto")) s->down = 1;
        else expect_kw("to");
        s->b = parse_expr();
        expect_kw("do");
        s->s1 = parse_stmt();
        return s;
    }
    if (accept_kw("case")) {
        Stmt *s = new_stmt(S_CASE, p);
        s->a = parse_expr();
        expect_kw("of");
        for (;;) {
            if (accept_kw("else")) {
                s->else_ = parse_stmt_list("end");
                s->has_else = 1;
                break;
            }
            if (is_kw("end")) break;
            CaseArm arm = {0};
            for (;;) {
                if (is_kw("some") || is_kw("ok") || is_kw("error")) { /* some x / ok v / error e: unpacking patterns */
                    Expr *sp = new_expr(E_SOMEPAT, pos());
                    sp->op = is_kw("some") ? 0 : is_kw("ok") ? 1 : 2;
                    next();
                    if (sp->op != 1 || CUR.kind == TK_IDENT) {
                        sp->id = ident();
                        sp->has_id = 1;
                    }
                    vpush(arm.lo, sp);
                } else
                    vpush(arm.lo, parse_expr());
                vpush(arm.hi, accept_sym("..") ? parse_expr() : NULL);
                if (!accept_sym(",")) break;
            }
            expect_sym(":");
            arm.body = parse_stmt();
            vpush(s->arms, arm);
            if (!accept_sym(";")) {
                if (is_kw("else")) continue;
                break;
            }
        }
        expect_kw("end");
        return s;
    }
    if (accept_kw("with")) {
        Stmt *s = new_stmt(S_WITH, p);
        vpush(s->exprs, parse_expr());
        while (accept_sym(",")) vpush(s->exprs, parse_expr());
        expect_kw("do");
        s->s1 = parse_stmt();
        return s;
    }
    if (accept_kw("try")) { /* try P(x); : stop with the error of a failed result */
        Stmt *s = new_stmt(S_CALL, p);
        Expr *e = new_expr(E_TRY, p);
        e->a = parse_postfix();
        s->a = e;
        return s;
    }
    if (is_kw("goto")) error_at(p, "goto is not supported");
    Expr *lhs = parse_postfix();
    if (accept_sym(":=")) {
        Stmt *s = new_stmt(S_ASSIGN, p);
        s->a = lhs;
        s->b = parse_expr();
        return s;
    }
    Stmt *s = new_stmt(S_CALL, p);
    s->a = lhs;
    return s;
}

/* ------------------------------------------------------------- expressions */

static Expr *binop(int op, Expr *l, Expr *r, Pos p) {
    Expr *e = new_expr(E_BINARY, p);
    e->op = op;
    e->a = l;
    e->b = r;
    return e;
}

static Expr *parse_factor(void);

static Expr *parse_expr(void) {
    Expr *l = parse_simple();
    Pos p = pos();
    int op = -1;
    if (is_sym("=")) op = OP_EQ;
    else if (is_sym("<>")) op = OP_NE;
    else if (is_sym("<")) op = OP_LT;
    else if (is_sym("<=")) op = OP_LE;
    else if (is_sym(">")) op = OP_GT;
    else if (is_sym(">=")) op = OP_GE;
    if (op >= 0) {
        next();
        return binop(op, l, parse_simple(), p);
    }
    if (accept_kw("in")) {
        Expr *e = new_expr(E_IN, p);
        e->a = l;
        e->b = parse_simple();
        return e;
    }
    if (is_kw("is") && T.items[P + 1].kind == TK_KW &&
        (STREQ(T.items[P + 1].s, "some") || STREQ(T.items[P + 1].s, "ok") || STREQ(T.items[P + 1].s, "error"))) {
        next();
        Expr *e = new_expr(E_ISSOME, p);
        e->op = is_kw("some") ? 0 : is_kw("ok") ? 1 : 2;
        next();
        e->a = l;
        if (e->op != 1 || CUR.kind == TK_IDENT) {
            e->id = ident();
            e->has_id = 1;
        }
        return e;
    }
    if (accept_kw("is")) {
        Expr *e = new_expr(E_IS, p);
        e->a = l;
        e->id = ident();
        return e;
    }
    return l;
}

static Expr *parse_simple(void) {
    Expr *l = parse_factor();
    for (;;) {
        Pos p = pos();
        int op;
        if (is_sym("+")) op = OP_ADD;
        else if (is_sym("-")) op = OP_SUB;
        else if (is_kw("or")) op = OP_OR;
        else if (is_kw("xor")) op = OP_XOR;
        else break;
        next();
        Expr *r = parse_factor();
        l = binop(op, l, r, p);
    }
    return l;
}

/* factor followed by multiplicative operators */
static Expr *parse_unary(void);
static Expr *parse_factor(void) {
    Expr *l = parse_unary();
    for (;;) {
        Pos q = pos();
        int op2;
        if (is_sym("*")) op2 = OP_MUL;
        else if (is_sym("/")) op2 = OP_DIV;
        else if (is_kw("div")) op2 = OP_IDIV;
        else if (is_kw("mod")) op2 = OP_MOD;
        else if (is_kw("and")) op2 = OP_AND;
        else if (is_kw("shl")) op2 = OP_SHL;
        else if (is_kw("shr")) op2 = OP_SHR;
        else if (is_kw("as")) {
            next();
            Expr *e = new_expr(E_AS, q);
            e->a = l;
            e->id = ident();
            l = e;
            continue;
        } else break;
        next();
        l = binop(op2, l, parse_unary(), q);
    }
    return l;
}

static Expr *parse_unary(void) {
    Pos p = pos();
    if (accept_kw("not")) {
        Expr *e = new_expr(E_UNARY, p);
        e->op = OP_NOT;
        e->a = parse_unary();
        return e;
    }
    if (accept_sym("-")) {
        Expr *e = new_expr(E_UNARY, p);
        e->op = OP_NEG;
        e->a = parse_unary();
        return e;
    }
    if (accept_sym("+")) {
        Expr *e = new_expr(E_UNARY, p);
        e->op = OP_PLUS;
        e->a = parse_unary();
        return e;
    }
    if (is_sym("@"))
        error_at(p, "the address operator '@' is not supported: use a var parameter or a borrowed reference");
    if (accept_sym("[")) {
        Expr *e = new_expr(E_SET, p);
        if (!is_sym("]")) {
            for (;;) {
                vpush(e->args, parse_expr());
                vpush(e->args2, accept_sym("..") ? parse_expr() : NULL);
                if (!accept_sym(",")) break;
            }
        }
        expect_sym("]");
        return e;
    }
    return parse_postfix();
}

static void parse_args(ExprVec *args) {
    if (accept_sym(")")) return;
    for (;;) {
        Pos p = pos();
        Expr *e = parse_expr();
        if (accept_sym(":")) {
            Expr *f = new_expr(E_FMT, p);
            f->a = e;
            f->b = parse_expr();
            if (accept_sym(":")) f->c = parse_expr();
            e = f;
        }
        vpush(*args, e);
        if (!accept_sym(",")) break;
    }
    expect_sym(")");
}

static Expr *parse_primary(void) {
    Pos p = pos();
    Expr *e;
    switch (CUR.kind) {
    case TK_INT:
        e = new_expr(E_INT, p);
        e->ival = CUR.ival;
        next();
        return e;
    case TK_REAL:
        e = new_expr(E_REAL, p);
        e->rval = CUR.rval;
        next();
        return e;
    case TK_STR:
        e = new_expr(E_STR, p);
        e->sval = CUR.str;
        e->slen = CUR.slen;
        next();
        return e;
    case TK_IDENT:
        e = new_expr(E_IDENT, p);
        e->id = ident();
        if (generic_args_ahead()) e->targs = parse_targs(); /* TListe<TKunde>.Create */
        return e;
    default: break;
    }
    if (accept_kw("none")) return new_expr(E_NIL, p);
    if (is_kw("nil")) error_at(p, "'nil' does not exist: optional values are empty with 'none'");
    if (accept_kw("default")) return new_expr(E_DEFAULT, p);
    if (is_kw("ok") || is_kw("error")) { /* ok(v), ok, error(e) */
        Expr *e = new_expr(is_kw("ok") ? E_OK : E_ERR, p);
        next();
        if (e->kind == E_ERR || (CUR.kind == TK_SYM && STREQ(CUR.s, "("))) {
            expect_sym("(");
            e->a = parse_expr();
            expect_sym(")");
        }
        return e;
    }
    if (accept_kw("try")) { /* try r: unpack a result or return its error */
        Expr *e = new_expr(E_TRY, p);
        e->a = parse_postfix();
        return e;
    }
    if (accept_sym("(")) {
        if (CUR.kind == TK_IDENT && T.items[P + 1].kind == TK_SYM && STREQ(T.items[P + 1].s, ":")) {
            /* record literal (X: 1; Y: 2) */
            e = new_expr(E_RECLIT, p);
            for (;;) {
                vpush(e->fnames, ident());
                expect_sym(":");
                vpush(e->args, parse_expr());
                if (!accept_sym(";")) break;
                if (is_sym(")")) break;
            }
            expect_sym(")");
            return e;
        }
        e = parse_expr();
        if (is_sym(",")) { /* array literal (1, 2, 3) */
            Expr *l = new_expr(E_LIST, p);
            vpush(l->args, e);
            while (accept_sym(",")) vpush(l->args, parse_expr());
            expect_sym(")");
            return l;
        }
        expect_sym(")");
        return e;
    }
    if (accept_kw("inherited")) {
        e = new_expr(E_INHERITED, p);
        if (CUR.kind == TK_IDENT) {
            e->id = ident();
            e->has_id = 1;
            if (accept_sym("(")) {
                e->has_args = 1;
                parse_args(&e->args);
            }
        }
        return e;
    }
    error_at(p, "expression expected but %s found", describe());
}

static Expr *parse_postfix(void) {
    Expr *e = parse_primary();
    for (;;) {
        Pos p = pos();
        if (is_sym("^")) {
            error_at(p, "pointer dereference '^' is not supported: class references are used directly (x.Field)");
        } else if (is_sym(".") && peek_is_ident(1)) {
            next();
            Expr *m = new_expr(E_MEMBER, p);
            m->a = e;
            m->id = ident();
            e = m;
        } else if (accept_sym("[")) {
            Expr *x = new_expr(E_INDEX, p);
            x->a = e;
            vpush(x->args, parse_expr());
            while (accept_sym(",")) vpush(x->args, parse_expr());
            expect_sym("]");
            e = x;
        } else if (is_sym("(") && e->kind != E_INHERITED) {
            next();
            Expr *c = new_expr(E_CALL, p);
            c->a = e;
            parse_args(&c->args);
            e = c;
        } else
            break;
    }
    return e;
}

/* ------------------------------------------------------------- entry points */

static Block *parse_block(void) {
    Block *b = NEW(Block);
    parse_decls(b);
    expect_kw("begin");
    b->body = parse_stmt_list("end");
    expect_kw("end");
    return b;
}

static IdentVec parse_imports(void) {
    IdentVec v = {0};
    if (is_kw("uses")) error_at(pos(), "'uses' was replaced by 'import' (for example 'import ppc_std;')");
    if (accept_kw("import")) {
        v = ident_list();
        expect_sym(";");
    }
    return v;
}

/* module Name; [import ...;] declarations end. */
Block *parse_module(const char *src, Ident *name) {
    T = tokenize(src);
    cur_toks = NEW(TokVec);
    *cur_toks = T;
    P = 0;
    if (!is_kw("module")) error_at(pos(), "a module starts with 'module Name;'");
    next();
    *name = ident();
    expect_sym(";");
    Block *b = NEW(Block);
    b->uses = parse_imports();
    if (!is_kw("interface"))
        error_at(pos(), "a module has an 'interface' section (the public declarations) and an 'implementation' section (their definitions)");
    next();
    in_interface = 1;
    parse_decls(b); /* public: types, constants, variables, routine headers */
    in_interface = 0;
    if (!is_kw("implementation"))
        error_at(pos(), "'implementation' expected after the interface section (found %s)", describe());
    next();
    b->impl = NEW(Block);
    parse_decls(b->impl); /* definitions and private declarations */
    if (is_kw("begin")) error_at(pos(), "a module has no main block ('begin ... end'): it ends with 'end.'");
    expect_kw("end");
    expect_sym(".");
    if (CUR.kind != TK_EOF) error_at(pos(), "unexpected %s after the end of the module", describe());
    return b;
}

Block *parse_program(const char *src, char **name, Pos *end_pos) {
    T = tokenize(src);
    cur_toks = NEW(TokVec);
    *cur_toks = T;
    P = 0;
    *name = xstrdup("main");
    if (is_kw("module")) error_at(pos(), "this file is a module ('module %s;'), not a program: import it from a program", T.items[P + 1].s);
    if (accept_kw("program")) {
        *name = ident().name;
        if (accept_sym("(")) {
            ident_list();
            expect_sym(")");
        }
        expect_sym(";");
    }
    IdentVec uses = parse_imports();
    Block *b = NEW(Block);
    b->uses = uses;
    parse_decls(b);
    expect_kw("begin");
    b->body = parse_stmt_list("end");
    *end_pos = pos();
    expect_kw("end");
    expect_sym(".");
    return b;
}

/* after an identifier: '<' types '>' followed by '.' or '(' -- type arguments, not a comparison */
static int generic_args_ahead(void) {
    if (!(CUR.kind == TK_SYM && STREQ(CUR.s, "<"))) return 0;
    int depth = 0;
    for (int i = P; i < T.len; i++) {
        Token *t = &T.items[i];
        if (t->kind == TK_SYM && STREQ(t->s, "<")) depth++;
        else if (t->kind == TK_SYM && STREQ(t->s, ">")) {
            if (--depth == 0) {
                Token *n = &T.items[i + 1 < T.len ? i + 1 : i];
                return n->kind == TK_SYM && (STREQ(n->s, ".") || STREQ(n->s, "("));
            }
        } else if (t->kind == TK_SYM && STREQ(t->s, ",")) {
        } else if (t->kind == TK_IDENT || (t->kind == TK_KW && (STREQ(t->s, "optional") || STREQ(t->s, "result") || STREQ(t->s, "of")))) {
        } else
            return 0;
    }
    return 0;
}

/* parse a generic type or method again (one fresh syntax tree per instance) */
TypeExpr *reparse_type(void *toks, int start) {
    TokVec st = T;
    int sp = P;
    TokVec *sc = cur_toks;
    T = *(TokVec *)toks;
    cur_toks = toks;
    P = start;
    TypeExpr *t = parse_type();
    T = st;
    P = sp;
    cur_toks = sc;
    return t;
}
RoutineDecl *reparse_routine(void *toks, int start) {
    TokVec st = T;
    int sp = P;
    TokVec *sc = cur_toks;
    T = *(TokVec *)toks;
    cur_toks = toks;
    P = start;
    RoutineDecl *d = parse_routine();
    T = st;
    P = sp;
    cur_toks = sc;
    return d;
}

Block *parse_decls_only(const char *src) {
    T = tokenize(src);
    cur_toks = NEW(TokVec);
    *cur_toks = T;
    P = 0;
    Block *b = NEW(Block);
    parse_decls(b);
    if (CUR.kind != TK_EOF) error_at(pos(), "unexpected %s", describe());
    return b;
}
