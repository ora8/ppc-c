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
    "inherited", "interface", "is", "label", "mod", "nil", "not", "object", "of", "or", "packed",
    "procedure", "program", "property", "record", "repeat", "set", "shl", "shr", "then", "to",
    "type", "unit", "until", "uses", "var", "while", "with", "xor", "try", "except", "finally",
    "raise", "otherwise", "heap", NULL};

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
        Pos pos = {line, col};
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
    e.pos = (Pos){line, col};
    vpush(out, e);
    return out;
#undef ADV
}

/* ================================================================ parser */

static TokVec T;
static int P;

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
static Block *parse_block(void);
static void parse_decls(Block *blk);

/* ------------------------------------------------------------- types */

static TypeExpr *new_te(TEKind k, Pos p) {
    TypeExpr *t = NEW(TypeExpr);
    t->kind = k;
    t->pos = p;
    return t;
}

static int accept_borrowed(void) {
    if (is_ident("borrowed")) {
        next();
        return 1;
    }
    return 0;
}

static const char *DIRECTIVES[] = {"virtual", "override", "abstract", "overload", "reintroduce",
                                   "static", "inline", "cdecl", "forward", "register", NULL};

static int is_directive(void) {
    if (CUR.kind != TK_IDENT) return 0;
    for (int k = 0; DIRECTIVES[k]; k++)
        if (STREQ(CUR.s, DIRECTIVES[k])) return 1;
    return 0;
}

static void parse_directives(int *virt, int *abs, int *fwd) {
    while (is_directive()) {
        const char *d = CUR.s;
        next();
        if (STREQ(d, "virtual")) *virt = VIRT_VIRTUAL;
        else if (STREQ(d, "override")) *virt = VIRT_OVERRIDE;
        else if (STREQ(d, "abstract")) *abs = 1;
        else if (STREQ(d, "forward")) *fwd = 1;
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
        else if (accept_kw("const")) g.mode = PM_CONST;
        else if (is_ident("out")) {
            next();
            g.mode = PM_OUT;
        } else if (is_ident("owned")) {
            next();
            g.mode = PM_OWNED;
        } else
            g.mode = PM_VALUE;
        g.names = ident_list();
        if (!accept_sym(":")) error_at(pos(), "untyped parameters are not supported");
        if (is_kw("array")) error_at(pos(), "array types in parameter lists must be named (declare a type; open arrays are not supported)");
        g.ty = parse_type();
        if (is_sym("=")) error_at(pos(), "default parameter values are not supported");
        vpush(v, g);
        if (!accept_sym(";")) break;
    }
    expect_sym(")");
    return v;
}

static int routine_kind(void) {
    int k = RK_PROC;
    if (is_kw("function")) k = RK_FUNC;
    else if (is_kw("constructor")) k = RK_CTOR;
    else if (is_kw("destructor")) k = RK_DTOR;
    next();
    return k;
}

static int is_routine_kw(void) {
    return is_kw("procedure") || is_kw("function") || is_kw("constructor") || is_kw("destructor");
}

static StructDecl *parse_struct(int kind) {
    StructDecl *sd = NEW(StructDecl);
    sd->kind = kind;
    if (accept_sym("(")) {
        sd->parent = ident();
        sd->has_parent = 1;
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
        if (accept_kw("var")) continue;
        if (is_kw("class")) error_at(pos(), "class methods/fields are not supported");
        if (is_routine_kw()) {
            MethodDecl m = {0};
            m.kind = routine_kind();
            m.name = ident();
            m.params = parse_params();
            if (m.kind == RK_FUNC) {
                expect_sym(":");
                m.ret_borrowed = accept_borrowed();
                m.ret = parse_type();
            }
            expect_sym(";");
            int fwd = 0;
            parse_directives(&m.virt, &m.is_abstract, &fwd);
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
            expect_sym(";");
            vpush(sd->fields, f);
            continue;
        }
        error_at(pos(), "unexpected %s in class declaration", describe());
    }
    return sd;
}

static TypeExpr *parse_type(void) {
    Pos p = pos();
    if (is_ident("nilable")) {
        next();
        TypeExpr *t = parse_type();
        t->nilable = 1;
        return t;
    }
    if (accept_sym("^")) {
        TypeExpr *t = new_te(TE_PTR, p);
        t->name = ident();
        return t;
    }
    accept_kw("packed");
    if (accept_kw("array")) {
        TypeExpr *t = new_te(TE_ARRAY, p);
        if (!accept_sym("[")) error_at(pos(), "dynamic arrays ('array of') are not supported");
        vpush(t->ranges, parse_type());
        while (accept_sym(",")) vpush(t->ranges, parse_type());
        expect_sym("]");
        expect_kw("of");
        t->elem = parse_type();
        return t;
    }
    if (accept_kw("record")) {
        TypeExpr *t = new_te(TE_RECORD, p);
        while (!is_kw("end")) {
            if (is_kw("case")) error_at(pos(), "variant records are not supported");
            FieldDecl f = {0};
            f.names = ident_list();
            expect_sym(":");
            f.borrowed = accept_borrowed();
            f.ty = parse_type();
            vpush(t->fields, f);
            if (!accept_sym(";")) break;
        }
        expect_kw("end");
        return t;
    }
    if (is_kw("object"))
        error_at(p, "'object' types are not supported: use 'class' (class variables in 'var' are stack objects, in 'heap' heap objects)");
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

static Expr *parse_init(void) {
    if (is_sym("(")) {
        int save = P;
        Pos p = pos();
        next();
        Expr *l = new_expr(E_LIST, p);
        vpush(l->args, parse_init());
        while (accept_sym(",")) vpush(l->args, parse_init());
        if (l->args.len > 1 && accept_sym(")")) return l;
        P = save;
    }
    return parse_expr();
}

static RoutineDecl *parse_routine(void) {
    RoutineDecl *d = NEW(RoutineDecl);
    d->pos = pos();
    d->kind = routine_kind();
    d->name = ident();
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
    parse_directives(&virt, &abs, &d->forward);
    if (d->forward) return d;
    if (is_ident("external")) error_at(pos(), "external routines are not supported");
    d->body = parse_block();
    expect_sym(";");
    return d;
}

static void parse_decls(Block *blk) {
    for (;;) {
        if (accept_kw("const")) {
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
                expect_sym("=");
                t.ty = parse_type();
                expect_sym(";");
                vpush(d->types, t);
            }
            vpush(blk->decls, d);
        } else if (accept_kw("var")) {
            Decl *d = NEW(Decl);
            d->kind = D_VAR;
            while (CUR.kind == TK_IDENT) {
                VarDecl v = {0};
                v.names = ident_list();
                expect_sym(":");
                v.borrowed = accept_borrowed();
                v.ty = parse_type();
                if (accept_sym("=")) v.init = parse_init();
                expect_sym(";");
                vpush(d->vars, v);
            }
            vpush(blk->decls, d);
        } else if (accept_kw("heap")) {
            /* heap section: owning references to heap objects (created with TClass.Create, freed with Free) */
            Decl *d = NEW(Decl);
            d->kind = D_VAR;
            while (CUR.kind == TK_IDENT) {
                VarDecl v = {0};
                v.names = ident_list();
                expect_sym(":");
                if (is_ident("borrowed"))
                    error_at(pos(), "a heap section declares owners; borrowed references belong into 'var'");
                v.ty = parse_type();
                v.is_heap = 1;
                if (accept_sym("=")) v.init = parse_init();
                expect_sym(";");
                vpush(d->vars, v);
            }
            if (!d->vars.len) error_at(pos(), "identifier expected in heap section");
            vpush(blk->decls, d);
        } else if (is_kw("label")) {
            error_at(pos(), "labels/goto are not supported");
        } else if (is_routine_kw()) {
            Decl *d = NEW(Decl);
            d->kind = D_ROUTINE;
            d->rt = parse_routine();
            vpush(blk->decls, d);
        } else
            break;
    }
}

/* ------------------------------------------------------------- statements */

static StmtVec parse_stmt_list(const char *term) {
    StmtVec v = {0};
    for (;;) {
        Stmt *s = parse_stmt();
        if (s->kind != S_EMPTY) vpush(v, s);
        if (accept_sym(";")) continue;
        if (is_kw(term)) break;
        error_at(pos(), "';' expected but %s found", describe());
    }
    return v;
}

static Stmt *parse_stmt(void) {
    Pos p = pos();
    if (is_sym(";") || is_kw("end") || is_kw("until") || is_kw("else")) return new_stmt(S_EMPTY, p);
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
        Stmt *s = new_stmt(S_WHILE, p);
        s->a = parse_expr();
        expect_kw("do");
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
            if (accept_kw("else") || accept_kw("otherwise")) {
                s->else_ = parse_stmt_list("end");
                s->has_else = 1;
                break;
            }
            if (is_kw("end")) break;
            CaseArm arm = {0};
            for (;;) {
                vpush(arm.lo, parse_expr());
                vpush(arm.hi, accept_sym("..") ? parse_expr() : NULL);
                if (!accept_sym(",")) break;
            }
            expect_sym(":");
            arm.body = parse_stmt();
            vpush(s->arms, arm);
            if (!accept_sym(";")) {
                if (is_kw("else") || is_kw("otherwise")) continue;
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
    if (is_kw("try") || is_kw("raise")) error_at(p, "exceptions (try/raise) are not supported");
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
    if (accept_sym("@")) {
        Expr *e = new_expr(E_ADDR, p);
        e->a = parse_postfix();
        return e;
    }
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
        return e;
    default: break;
    }
    if (accept_kw("nil")) return new_expr(E_NIL, p);
    if (accept_sym("(")) {
        e = parse_expr();
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
        if (accept_sym("^")) {
            Expr *d = new_expr(E_DEREF, p);
            d->a = e;
            e = d;
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

Block *parse_program(const char *src, char **name, Pos *end_pos) {
    T = tokenize(src);
    P = 0;
    *name = xstrdup("main");
    if (accept_kw("program")) {
        *name = ident().name;
        if (accept_sym("(")) {
            ident_list();
            expect_sym(")");
        }
        expect_sym(";");
    }
    if (accept_kw("uses")) {
        ident_list();
        expect_sym(";");
    }
    Block *b = NEW(Block);
    parse_decls(b);
    expect_kw("begin");
    b->body = parse_stmt_list("end");
    *end_pos = pos();
    expect_kw("end");
    expect_sym(".");
    return b;
}

Block *parse_decls_only(const char *src) {
    T = tokenize(src);
    P = 0;
    Block *b = NEW(Block);
    parse_decls(b);
    if (CUR.kind != TK_EOF) error_at(pos(), "unexpected %s", describe());
    return b;
}
