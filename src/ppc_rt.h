/* ---- ppc runtime (embedded into every generated program) ---- */
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <locale.h>
#include <wctype.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct { int64_t len; unsigned char *p; } PStr;
typedef struct { void *fn; void *env; } __Closure;
/* set of T: one bit per value (at most 256), bit i stands for the value lo + i */
typedef struct { uint64_t w[4]; } __Set; /* 'reference to function': the routine and the frame it uses (NULL: none) */ /* String: length + heap buffer (see strings) */
/* interface tables of a class: the methods of each interface it implements, ended by {0, 0} */
struct __ITab { const void *iid; void **fns; };
struct __VMTHdr { void *parent; const char *name; size_t size; void (*drop)(void *); void (*clonefix)(void *); const struct __ITab *itabs; };
/* drop: frees the owning fields; clonefix: after copying an object, replaces its owned objects by clones */

static void __rt_error(int code, const char *msg) {
    fflush(stdout);
    fprintf(stderr, "Runtime error %d: %s\n", code, msg);
    exit(code);
}
static void __ppc_assert_failed(const char *file, int line, int col, PStr msg) { /* Assert(False, ...) */
    fflush(stdout);
    fprintf(stderr, "Runtime error 227: assertion failed at %s(%d,%d)%s%.*s\n", file, line, col, msg.len ? ": " : "", (int)msg.len, (const char *)msg.p);
    exit(227);
}
static void *__ppc_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) __rt_error(203, "heap overflow");
    return p;
}
static void __ppc_free(void *p) { free(p); }
static void *__ppc_realloc(void *p, size_t n) { /* dynamic arrays: SetLength */
    if (!n) {
        free(p);
        return NULL;
    }
    void *q = realloc(p, n);
    if (!q) __rt_error(203, "heap overflow");
    return q;
}
static void *__nilref(void *c) { if (!c) __rt_error(210, "the class reference is not set"); return c; }
static void __abstract_err(void) { __rt_error(211, "abstract method called"); }
static int __is(void *o, void *vmt) {
    if (!o) return 0;
    struct __VMTHdr *v = *(struct __VMTHdr **)o;
    while (v) {
        if ((void *)v == vmt) return 1;
        v = (struct __VMTHdr *)v->parent;
    }
    return 0;
}
static void *__as(void *o, void *vmt) {
    if (o && !__is(o, vmt)) __rt_error(219, "invalid type cast");
    return o;
}
static void **__itab_find(void *o, const void *iid) {
    if (!o) return 0;
    const struct __ITab *t = (*(struct __VMTHdr **)o)->itabs;
    for (; t && t->iid; t++)
        if (t->iid == iid) return t->fns;
    return 0;
}
static void **__itab(void *o, const void *iid) {
    void **f = __itab_find(o, iid);
    if (!f) __rt_error(219, "object does not implement the interface");
    return f;
}
static int __implements(void *o, const void *iid) { return __itab_find(o, iid) != 0; }
static void *__as_iface(void *o, const void *iid) {
    if (o && !__itab_find(o, iid)) __rt_error(219, "invalid type cast: the object does not implement the interface");
    return o;
}
static const char *__ppc_srcfile;
static int64_t __rangechk(int64_t i, int64_t lo, int64_t hi, int line, int col) {
    if (i < lo || i > hi) {
        fflush(stdout);
        fprintf(stderr, "Runtime error 201: range check error at %s(%d,%d): index %lld is outside %lld..%lld\n",
                __ppc_srcfile, line, col, (long long)i, (long long)lo, (long long)hi);
        exit(201);
    }
    return i;
}
static void *__nilchk(void *p, int line, int col) {
    if (!p) {
        fflush(stdout);
        fprintf(stderr, "Runtime error 210: nil pointer dereference at %s(%d,%d)\n", __ppc_srcfile, line, col);
        exit(210);
    }
    return p;
}

/* ---- strings ----
 * A String is { len, p }. Every variable owns its own heap buffer (copied on assignment, freed at
 * the end of its scope). Results of string operations are temporaries: they are registered in a
 * pool that is emptied after every statement (down to the mark of the running routine).
 * Literals are views of static memory and are never freed. */
static PStr *__tp;
static size_t __tpn, __tpc;
static PStr __ppc_tmp(PStr s) {
    if (!s.p) return s;
    if (__tpn == __tpc) {
        __tpc = __tpc ? __tpc * 2 : 64;
        __tp = (PStr *)realloc(__tp, __tpc * sizeof(PStr));
        if (!__tp) __rt_error(203, "heap overflow");
    }
    __tp[__tpn++] = s;
    return s;
}
static size_t __ppc_mark(void) { return __tpn; }
static void __ppc_release(size_t m) {
    while (__tpn > m) free(__tp[--__tpn].p);
}
static PStr __pstr_new(int64_t n) { /* fresh owned buffer (not registered) */
    PStr r;
    r.len = n;
    r.p = n ? (unsigned char *)__ppc_alloc((size_t)n + 1) : NULL;
    return r;
}
static PStr __pstr_dup(PStr s) { /* an owned copy */
    PStr r = __pstr_new(s.len);
    if (s.len) memcpy(r.p, s.p, (size_t)s.len);
    return r;
}
static void __pstr_free(PStr *s) {
    free(s->p);
    s->p = NULL;
    s->len = 0;
}
static PStr __pstr_lit(const char *c, int64_t n) { PStr r; r.len = n; r.p = n ? (unsigned char *)c : NULL; return r; }
static PStr __pstr_char(unsigned char c) { PStr r = __pstr_new(1); r.p[0] = c; return __ppc_tmp(r); }
static PStr __pstr_cstr(const char *c) { return __pstr_lit(c, (int64_t)strlen(c)); }
static PStr __pstr_mem(const char *c, int64_t n) { PStr r = __pstr_new(n); if (n) memcpy(r.p, c, (size_t)n); return __ppc_tmp(r); }
static PStr __pstr_cat(PStr a, PStr b) {
    PStr r = __pstr_new(a.len + b.len);
    if (a.len) memcpy(r.p, a.p, (size_t)a.len);
    if (b.len) memcpy(r.p + a.len, b.p, (size_t)b.len);
    return __ppc_tmp(r);
}
static int __pstr_cmp(PStr a, PStr b) {
    int64_t k = a.len < b.len ? a.len : b.len;
    int c = k ? memcmp(a.p, b.p, (size_t)k) : 0;
    if (c) return c;
    return a.len < b.len ? -1 : a.len > b.len;
}
static PStr __pstr_copy(PStr s, int64_t i, int64_t c) {
    if (i < 1) i = 1;
    if (i > s.len || c <= 0) return __pstr_lit("", 0);
    if (c > s.len - i + 1) c = s.len - i + 1;
    return __pstr_mem((const char *)s.p + i - 1, c);
}
static int32_t __pstr_pos(PStr sub, PStr s) {
    if (sub.len == 0) return 0;
    for (int64_t i = 0; i + sub.len <= s.len; i++)
        if (memcmp(s.p + i, sub.p, (size_t)sub.len) == 0) return (int32_t)(i + 1);
    return 0;
}
static void __pstr_setlen(PStr *s, int64_t n) {
    if (n < 0) n = 0;
    if (!n) { __pstr_free(s); return; }
    s->p = (unsigned char *)realloc(s->p, (size_t)n + 1);
    if (!s->p) __rt_error(203, "heap overflow");
    if (n > s->len) memset(s->p + s->len, ' ', (size_t)(n - s->len));
    s->len = n;
}
static void __pstr_delete(PStr *s, int64_t i, int64_t c) {
    if (i < 1 || i > s->len || c <= 0) return;
    if (c > s->len - i + 1) c = s->len - i + 1;
    memmove(s->p + i - 1, s->p + i - 1 + c, (size_t)(s->len - (i - 1) - c));
    __pstr_setlen(s, s->len - c);
}
static void __pstr_insert(PStr src, PStr *s, int64_t i) {
    if (!src.len) return;
    if (i < 1) i = 1;
    if (i > s->len + 1) i = s->len + 1;
    int64_t old = s->len;
    PStr copy = __pstr_dup(src); /* src may point into s */
    __pstr_setlen(s, old + copy.len);
    memmove(s->p + i - 1 + copy.len, s->p + i - 1, (size_t)(old - (i - 1)));
    memcpy(s->p + i - 1, copy.p, (size_t)copy.len);
    free(copy.p);
}
static void __pstr_assign(PStr *dst, PStr src) { /* dst := src (an owned copy) */
    PStr n = __pstr_dup(src);
    free(dst->p);
    *dst = n;
}
static PStr __int_to_str(int64_t v) { char b[32]; int n = snprintf(b, sizeof b, "%lld", (long long)v); return __pstr_mem(b, n); }
static int32_t __str_to_int(PStr s) {
    char b[64];
    int64_t n = s.len < 63 ? s.len : 63;
    if (n) memcpy(b, s.p, (size_t)n);
    b[n] = 0;
    char *e;
    long long v = strtoll(b, &e, 10);
    if (s.len == 0 || *e || s.len > 63) {
        fflush(stdout);
        fprintf(stderr, "EConvertError: \"%s\" is an invalid integer\n", b);
        exit(1);
    }
    return (int32_t)v;
}
static PStr __float_to_str(double d) {
    char b[64];
    int n = snprintf(b, sizeof b, "%.15g", d);
    return __pstr_mem(b, n);
}
static PStr __upper(PStr s) {
    PStr r = __pstr_new(s.len);
    for (int64_t i = 0; i < s.len; i++) r.p[i] = (unsigned char)toupper(s.p[i]);
    return __ppc_tmp(r);
}
static PStr __lower(PStr s) {
    PStr r = __pstr_new(s.len);
    for (int64_t i = 0; i < s.len; i++) r.p[i] = (unsigned char)tolower(s.p[i]);
    return __ppc_tmp(r);
}
static unsigned char __upcase(unsigned char c) { return (unsigned char)toupper(c); }

/* ---- output ---- */
static void __w_pad(int64_t w, int len) { for (; len < w; len++) putchar(' '); }
static void __w_str(PStr s, int64_t w) { __w_pad(w, (int)s.len); if (s.len) fwrite(s.p, 1, (size_t)s.len, stdout); }
static void __w_cstr(const char *s, int64_t w) { __w_pad(w, (int)strlen(s)); fputs(s, stdout); }
static void __w_char(unsigned char c, int64_t w) { __w_pad(w, 1); putchar(c); }
/* ---- UTF-8: a UChar is a code point ---- */
static int __utf8_enc(uint32_t c, unsigned char *b) { /* its bytes (1..4); invalid code points become U+FFFD */
    if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) c = 0xFFFD;
    if (c < 0x80) { b[0] = (unsigned char)c; return 1; }
    if (c < 0x800) { b[0] = (unsigned char)(0xC0 | (c >> 6)); b[1] = (unsigned char)(0x80 | (c & 0x3F)); return 2; }
    if (c < 0x10000) { b[0] = (unsigned char)(0xE0 | (c >> 12)); b[1] = (unsigned char)(0x80 | ((c >> 6) & 0x3F)); b[2] = (unsigned char)(0x80 | (c & 0x3F)); return 3; }
    b[0] = (unsigned char)(0xF0 | (c >> 18)); b[1] = (unsigned char)(0x80 | ((c >> 12) & 0x3F)); b[2] = (unsigned char)(0x80 | ((c >> 6) & 0x3F)); b[3] = (unsigned char)(0x80 | (c & 0x3F));
    return 4;
}
/* the code point at byte i (0-based) of s, *len its bytes; an invalid byte is U+FFFD (1 byte) */
static uint32_t __utf8_dec(const unsigned char *s, int64_t n, int64_t i, int *len) {
    unsigned char c = s[i];
    int l = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (!l || i + l > n) { *len = 1; return c < 0x80 ? c : 0xFFFD; }
    uint32_t v = l == 1 ? c : l == 2 ? c & 0x1F : l == 3 ? c & 0x0F : c & 0x07;
    for (int k = 1; k < l; k++) {
        if ((s[i + k] >> 6) != 2) { *len = 1; return 0xFFFD; }
        v = (v << 6) | (s[i + k] & 0x3F);
    }
    *len = l;
    return v;
}
static PStr __pstr_uchar(uint32_t c) { unsigned char b[4]; int n = __utf8_enc(c, b); PStr r = __pstr_new(n); memcpy(r.p, b, (size_t)n); return __ppc_tmp(r); }
static void __w_uchar(uint32_t c, int64_t w) { unsigned char b[4]; int n = __utf8_enc(c, b); __w_pad(w, 1); fwrite(b, 1, (size_t)n, stdout); }
static void __w_int(int64_t v, int64_t w) { printf("%*lld", (int)w, (long long)v); }
static void __w_bool(int b, int64_t w) { __w_cstr(b ? "TRUE" : "FALSE", w); }
static void __fmt_real(char *out, size_t sz, double v, int64_t w, int64_t p) {
    if (p >= 0) { snprintf(out, sz, "%*.*f", (int)w, (int)p, v); return; }
    int digits = 16;
    if (w > 0) { digits = (int)w - 8; if (digits < 1) digits = 1; if (digits > 16) digits = 16; }
    char b[64];
    snprintf(b, sizeof b, "%.*E", digits, v);
    char *e = strchr(b, 'E');
    char mant[64];
    size_t ml = (size_t)(e - b);
    memcpy(mant, b, ml);
    mant[ml] = 0;
    int ex = atoi(e + 1);
    char t[96];
    snprintf(t, sizeof t, "%s%sE%c%03d", v < 0 || (v == 0 && signbit(v)) ? "" : " ", mant, ex < 0 ? '-' : '+', ex < 0 ? -ex : ex);
    snprintf(out, sz, "%*s", (int)w, t);
}
static void __w_real(double v, int64_t w, int64_t p) { char b[128]; __fmt_real(b, sizeof b, v, w, p); fputs(b, stdout); }
static void __w_ln(void) { putchar('\n'); }
static PStr __str_real(double v, int64_t w, int64_t p) { char b[128]; __fmt_real(b, sizeof b, v, w, p); return __pstr_mem(b, (int64_t)strlen(b)); }
static PStr __str_int(int64_t v, int64_t w) { char b[64]; snprintf(b, sizeof b, "%*lld", (int)w, (long long)v); return __pstr_mem(b, (int64_t)strlen(b)); }

/* ---- input ---- */
static int64_t __r_int(void) {
    long long v = 0;
    fflush(stdout);
    if (scanf("%lld", &v) != 1) __rt_error(106, "invalid numeric format");
    return v;
}
static double __r_real(void) {
    double v = 0;
    fflush(stdout);
    if (scanf("%lf", &v) != 1) __rt_error(106, "invalid numeric format");
    return v;
}
static unsigned char __r_char(void) {
    fflush(stdout);
    int c = getchar();
    if (c == EOF) return 26;
    return (unsigned char)c;
}
static PStr __r_str(void) { /* ReadLn(s): the rest of the line */
    PStr r = __pstr_new(0);
    int64_t cap = 0;
    int c;
    fflush(stdout);
    while ((c = getchar()) != EOF && c != '\n') {
        if (c == '\r') continue;
        if (r.len + 1 >= cap) {
            cap = cap ? cap * 2 : 64;
            r.p = (unsigned char *)realloc(r.p, (size_t)cap);
            if (!r.p) __rt_error(203, "heap overflow");
        }
        r.p[r.len++] = (unsigned char)c;
    }
    if (c == '\n') ungetc(c, stdin);
    return __ppc_tmp(r);
}
static void __r_ln(void) { int c; while ((c = getchar()) != EOF && c != '\n') {} }

/* ---- misc ---- */
static uint64_t __rnd_state = 88172645463325252ULL;
static uint64_t __rnd_next(void) { __rnd_state ^= __rnd_state << 13; __rnd_state ^= __rnd_state >> 7; __rnd_state ^= __rnd_state << 17; return __rnd_state; }
static int32_t __random_int(int64_t n) { if (n <= 0) return 0; return (int32_t)(__rnd_next() % (uint64_t)n); }
static double __random_real(void) { return (double)(__rnd_next() >> 11) / 9007199254740992.0; }
static void __randomize(void) { __rnd_state ^= (uint64_t)time(NULL) * 2654435761ULL; if (!__rnd_state) __rnd_state = 1; }
static int64_t __absi(int64_t v) { return v < 0 ? -v : v; }
static int64_t __sqri(int64_t v) { return v * v; }
static double __sqrr(double v) { return v * v; }
static int64_t __round(double v) { return (int64_t)llrint(v); }
static double __frac(double v) { return v - trunc(v); }
/* Format('%d items, %s', [n, s]): %[index:][-][width][.precision]type with d u x f e g n s and %% */
typedef struct { char kind; int64_t i; double r; PStr s; } __FArg; /* kind: 'i' whole number, 'r' real, 's' string */
static void __fput(char **o, size_t *len, size_t *cap, const char *s, size_t n) {
    if (*len + n + 1 > *cap) {
        while (*len + n + 1 > *cap) *cap = *cap ? *cap * 2 : 64;
        *o = (char *)realloc(*o, *cap);
    }
    memcpy(*o + *len, s, n);
    *len += n;
}
static PStr __format(PStr fmt, int n, const __FArg *a) {
    char *o = NULL;
    size_t len = 0, cap = 0;
    int next = 0;
    char tmp[512];
    for (int64_t i = 0; i < fmt.len; i++) {
        char c = (char)fmt.p[i];
        if (c != '%') { __fput(&o, &len, &cap, &c, 1); continue; }
        if (++i >= fmt.len) __rt_error(220, "Format: '%' at the end of the format");
        if (fmt.p[i] == '%') { __fput(&o, &len, &cap, "%", 1); continue; }
        int64_t j = i, num = 0, has = 0;
        while (j < fmt.len && isdigit(fmt.p[j])) num = num * 10 + (fmt.p[j++] - '0'), has = 1;
        if (has && j < fmt.len && fmt.p[j] == ':') { next = (int)num; i = j + 1; }
        int left = 0, width = 0, prec = -1;
        if (i < fmt.len && fmt.p[i] == '-') { left = 1; i++; }
        while (i < fmt.len && isdigit(fmt.p[i])) width = width * 10 + (fmt.p[i++] - '0');
        if (i < fmt.len && fmt.p[i] == '.') { prec = 0; i++; while (i < fmt.len && isdigit(fmt.p[i])) prec = prec * 10 + (fmt.p[i++] - '0'); }
        if (i >= fmt.len) __rt_error(220, "Format: incomplete format specifier");
        char t = (char)tolower(fmt.p[i]);
        if (next >= n) __rt_error(220, "Format: not enough arguments for the format");
        const __FArg *x = &a[next++];
        int k = 0;
        switch (t) {
        case 'd': case 'u': case 'x':
            if (x->kind != 'i') __rt_error(220, "Format: %d, %u and %x need a whole number");
            if (t == 'x') k = snprintf(tmp, sizeof tmp, left ? "%-*.*llX" : "%*.*llX", width, prec < 0 ? 1 : prec, (unsigned long long)x->i);
            else if (t == 'u') k = snprintf(tmp, sizeof tmp, left ? "%-*.*llu" : "%*.*llu", width, prec < 0 ? 1 : prec, (unsigned long long)x->i);
            else k = snprintf(tmp, sizeof tmp, left ? "%-*.*lld" : "%*.*lld", width, prec < 0 ? 1 : prec, (long long)x->i);
            break;
        case 'f': case 'e': case 'g': case 'n': {
            if (x->kind == 's') __rt_error(220, "Format: %f, %e, %g and %n need a number");
            double v = x->kind == 'i' ? (double)x->i : x->r;
            if (t == 'n') { /* thousands separated by ',' */
                char num_s[400];
                snprintf(num_s, sizeof num_s, "%.*f", prec < 0 ? 2 : prec, v);
                char *dot = strchr(num_s, '.'), *start = num_s + (num_s[0] == '-');
                int intlen = (int)((dot ? dot : num_s + strlen(num_s)) - start);
                char grp[512];
                int g = 0;
                if (num_s[0] == '-') grp[g++] = '-';
                for (int q = 0; q < intlen; q++) {
                    if (q && (intlen - q) % 3 == 0) grp[g++] = ',';
                    grp[g++] = start[q];
                }
                grp[g] = 0;
                if (dot) strcat(grp, dot);
                k = snprintf(tmp, sizeof tmp, left ? "%-*s" : "%*s", width, grp);
            } else {
                char f[8] = {'%', left ? '-' : '*', '*', '.', '*', t == 'e' ? 'E' : t, 0};
                if (left) { f[1] = '-'; f[2] = '*'; f[3] = '.'; f[4] = '*'; f[5] = t == 'e' ? 'E' : t; f[6] = 0; }
                else { f[1] = '*'; f[2] = '.'; f[3] = '*'; f[4] = t == 'e' ? 'E' : t; f[5] = 0; }
                k = snprintf(tmp, sizeof tmp, f, width, prec < 0 ? (t == 'f' ? 2 : 6) : prec, v);
            }
            break;
        }
        case 's': {
            if (x->kind != 's') __rt_error(220, "Format: %s needs a string");
            int64_t sl = prec >= 0 && prec < x->s.len ? prec : x->s.len;
            int pad = width > sl ? (int)(width - sl) : 0;
            if (!left) for (int q = 0; q < pad; q++) __fput(&o, &len, &cap, " ", 1);
            __fput(&o, &len, &cap, (const char *)x->s.p, (size_t)sl);
            if (left) for (int q = 0; q < pad; q++) __fput(&o, &len, &cap, " ", 1);
            continue;
        }
        default: __rt_error(220, "Format: unknown format specifier");
        }
        if (k > (int)sizeof tmp - 1) k = (int)sizeof tmp - 1;
        __fput(&o, &len, &cap, tmp, (size_t)k);
    }
    PStr r = __pstr_mem(o ? o : "", (int64_t)len);
    free(o);
    return r;
}
/* ---- hashing (Hash(x), hash maps) ---- */
static int64_t __hash_mix(uint64_t x) { x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33; return (int64_t)x; }
static int64_t __hash_str(PStr s) { uint64_t h = 1469598103934665603ULL; for (int64_t i = 0; i < s.len; i++) { h ^= s.p[i]; h *= 1099511628211ULL; } return __hash_mix(h); }
static int64_t __hash_real(double d) { uint64_t u; if (d == 0) d = 0; memcpy(&u, &d, sizeof u); return __hash_mix(u); }
static int64_t __hash_comb(int64_t a, int64_t b) { return __hash_mix((uint64_t)a * 1000003ULL ^ (uint64_t)b); }
/* ---- sets ---- */
static void __set_incl(__Set *s, int64_t v, int64_t lo, int64_t hi) {
    if (v < lo || v > hi) __rt_error(201, "range check error: set element out of range");
    v -= lo; s->w[v >> 6] |= (uint64_t)1 << (v & 63);
}
static void __set_excl(__Set *s, int64_t v, int64_t lo, int64_t hi) {
    if (v < lo || v > hi) return;
    v -= lo; s->w[v >> 6] &= ~((uint64_t)1 << (v & 63));
}
static void __set_range(__Set *s, int64_t a, int64_t b, int64_t lo, int64_t hi) {
    for (int64_t v = a; v <= b; v++) __set_incl(s, v, lo, hi);
}
static int __set_has(__Set s, int64_t v, int64_t lo, int64_t hi) {
    if (v < lo || v > hi) return 0;
    v -= lo; return (s.w[v >> 6] >> (v & 63)) & 1;
}
static __Set __set_or(__Set a, __Set b) { for (int i = 0; i < 4; i++) a.w[i] |= b.w[i]; return a; }
static __Set __set_and(__Set a, __Set b) { for (int i = 0; i < 4; i++) a.w[i] &= b.w[i]; return a; }
static __Set __set_sub(__Set a, __Set b) { for (int i = 0; i < 4; i++) a.w[i] &= ~b.w[i]; return a; }
static int __set_eq(__Set a, __Set b) { for (int i = 0; i < 4; i++) if (a.w[i] != b.w[i]) return 0; return 1; }
static int __set_le(__Set a, __Set b) { for (int i = 0; i < 4; i++) if (a.w[i] & ~b.w[i]) return 0; return 1; } /* a is a subset of b */
static int64_t __hash_set(__Set a) { int64_t h = 0; for (int i = 0; i < 4; i++) h = __hash_comb(h, (int64_t)a.w[i]); return h; }
static int64_t __set_count(__Set a) { int64_t n = 0; for (int i = 0; i < 4; i++) n += __builtin_popcountll(a.w[i]); return n; }
/* ---- standard library primitives (declared 'external' in std) ---- */
static int __ppc_argc;
static char **__ppc_argv;
static char *__cz(PStr s) { char *c = (char *)malloc((size_t)s.len + 1); if (s.len) memcpy(c, s.p, (size_t)s.len); c[s.len] = 0; return c; }
static int64_t __std_floor(double x) { return (int64_t)floor(x); }
static int64_t __std_ceil(double x) { return (int64_t)ceil(x); }
static double __std_power(double x, double y) { return pow(x, y); }
static double __std_tan(double x) { return tan(x); }
static double __std_arcsin(double x) { return asin(x); }
static double __std_arccos(double x) { return acos(x); }
static double __std_arctan2(double y, double x) { return atan2(y, x); }
static double __std_log10(double x) { return log10(x); }
static double __std_log2(double x) { return log2(x); }
static double __std_hypot(double x, double y) { return hypot(x, y); }
static uint8_t __std_isnan(double x) { return isnan(x) != 0; }
static int32_t __std_posfrom(PStr sub, PStr s, int64_t from) { /* first position of sub in s at or after from (1-based), 0 if none */
    if (from < 1) from = 1;
    if (sub.len == 0) return from <= s.len + 1 ? (int32_t)from : 0;
    for (int64_t i = from - 1; i + sub.len <= s.len; i++)
        if (s.p[i] == sub.p[0] && memcmp(s.p + i, sub.p, (size_t)sub.len) == 0) return (int32_t)(i + 1);
    return 0;
}
static PStr __std_inttohex(int64_t v, int32_t digits) { char b[40]; int n = snprintf(b, sizeof b, "%0*llX", digits < 0 ? 0 : digits > 16 ? 16 : digits, (unsigned long long)v); return __pstr_mem(b, n); }
static uint8_t __std_trystrtoint(PStr s, int64_t *v) {
    char *c = __cz(s), *end;
    char *b = c;
    while (isspace((unsigned char)*b)) b++;
    errno = 0;
    long long r = strtoll(b, &end, 10);
    int ok = *b && end != b && errno == 0;
    while (ok && isspace((unsigned char)*end)) end++;
    ok = ok && *end == 0;
    free(c);
    if (ok) *v = r;
    return ok;
}
static uint8_t __std_trystrtofloat(PStr s, double *v) {
    char *c = __cz(s), *end;
    char *b = c;
    while (isspace((unsigned char)*b)) b++;
    double r = strtod(b, &end);
    int ok = *b && end != b;
    while (ok && isspace((unsigned char)*end)) end++;
    ok = ok && *end == 0;
    free(c);
    if (ok) *v = r;
    return ok;
}
/* files: whole-file reading and writing; the last error message is kept for the Pascal wrappers */
static char __std_err[512];
static void __std_seterr(const char *what, PStr name) {
    char *n = __cz(name);
    snprintf(__std_err, sizeof __std_err, "%s '%s': %s", what, n, strerror(errno));
    free(n);
}
static PStr __std_ioerror(void) { return __pstr_mem(__std_err, (int64_t)strlen(__std_err)); }

/* ---- Unicode (UTF-8) strings: the U... routines of std ---- */
static int64_t __std_ulength(PStr s) { /* characters (code points) */
    int64_t n = 0;
    for (int64_t i = 0; i < s.len; n++) { int l; __utf8_dec(s.p, s.len, i, &l); i += l; }
    return n;
}
static int64_t __std_uoffset(PStr s, int64_t index) { /* the byte position (from 1) of character 'index' (from 1); Length+1 after the end */
    int64_t i = 0;
    for (int64_t k = 1; k < index && i < s.len; k++) { int l; __utf8_dec(s.p, s.len, i, &l); i += l; }
    return i + 1;
}
static int64_t __std_ucount(PStr s, int64_t bytes) { /* the characters in the first 'bytes' bytes */
    int64_t n = 0;
    for (int64_t i = 0; i < s.len && i < bytes; n++) { int l; __utf8_dec(s.p, s.len, i, &l); i += l; }
    return n;
}
static uint32_t __std_udecode(PStr s, int64_t pos, int64_t *len) { /* the character at byte 'pos' (from 1) and its length in bytes */
    if (pos < 1 || pos > s.len) __rt_error(201, "UChar position outside the string");
    int l;
    uint32_t c = __utf8_dec(s.p, s.len, pos - 1, &l);
    *len = l;
    return c;
}
static uint32_t __std_uat(PStr s, int64_t index) { /* character 'index' (from 1) */
    int64_t p = __std_uoffset(s, index), l;
    if (index < 1 || p > s.len) __rt_error(201, "UAt: the string has fewer characters");
    return __std_udecode(s, p, &l);
}
static uint32_t __std_uchr(int64_t code) {
    if (code < 0 || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) __rt_error(201, "UChr: not a Unicode code point");
    return (uint32_t)code;
}
static void __u_locale(void) { /* towupper and iswalpha need a UTF-8 locale for more than ASCII */
    static int done;
    if (done) return;
    done = 1;
    if (!setlocale(LC_CTYPE, "C.UTF-8") && !setlocale(LC_CTYPE, "C.utf8")) setlocale(LC_CTYPE, "en_US.UTF-8");
}
static uint32_t __std_uupcase(uint32_t c) { __u_locale(); return (uint32_t)towupper((wint_t)c); }
static uint32_t __std_ulowcase(uint32_t c) { __u_locale(); return (uint32_t)towlower((wint_t)c); }
static PStr __u_map(PStr s, int up) {
    __u_locale();
    PStr r = __pstr_new(s.len * 2 + 4);
    int64_t o = 0;
    for (int64_t i = 0; i < s.len;) {
        int l;
        uint32_t c = __utf8_dec(s.p, s.len, i, &l);
        i += l;
        c = (uint32_t)(up ? towupper((wint_t)c) : towlower((wint_t)c));
        if (o + 4 > r.len) { r.p = (unsigned char *)realloc(r.p, (size_t)(r.len *= 2) + 1); }
        o += __utf8_enc(c, r.p + o);
    }
    r.len = o;
    return __ppc_tmp(r);
}
static PStr __std_uuppercase(PStr s) { return __u_map(s, 1); }
static PStr __std_ulowercase(PStr s) { return __u_map(s, 0); }
static uint8_t __std_uisletter(uint32_t c) { __u_locale(); return iswalpha((wint_t)c) != 0; }
static uint8_t __std_uisdigit(uint32_t c) { __u_locale(); return iswdigit((wint_t)c) != 0; }
static uint8_t __std_uisspace(uint32_t c) { __u_locale(); return iswspace((wint_t)c) != 0; }
static uint8_t __std_uisupper(uint32_t c) { __u_locale(); return iswupper((wint_t)c) != 0; }
static uint8_t __std_uislower(uint32_t c) { __u_locale(); return iswlower((wint_t)c) != 0; }
static uint8_t __std_isvalidutf8(PStr s) {
    for (int64_t i = 0; i < s.len;) {
        int l;
        uint32_t c = __utf8_dec(s.p, s.len, i, &l);
        if (c == 0xFFFD && !(l == 3 && s.p[i] == 0xEF && s.p[i + 1] == 0xBF && s.p[i + 2] == 0xBD)) return 0;
        if (c >= 0xD800 && c <= 0xDFFF) return 0;
        i += l;
    }
    return 1;
}

/* ---- standard input and other programs ---- */
static uint8_t __std_eof(void) { int c = getchar(); if (c == EOF) return 1; ungetc(c, stdin); return 0; }
static PStr __std_readall(void) { /* the rest of standard input */
    size_t cap = 4096, len = 0, k;
    char *buf = (char *)malloc(cap);
    while ((k = fread(buf + len, 1, cap - len, stdin)) > 0) {
        len += k;
        if (len == cap) buf = (char *)realloc(buf, cap *= 2);
    }
    PStr r = __pstr_mem(buf, (int64_t)len);
    free(buf);
    return r;
}
static int32_t __std_runcommand(PStr cmd) { /* the exit code of the command (run by the shell), -1 if it could not run */
    fflush(stdout);
    char *c = __cz(cmd);
    int st = system(c);
    free(c);
    if (st == -1) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
}
static PStr __std_cmdoutput(PStr cmd) { /* the standard output of the command; __std_err when it failed */
    __std_err[0] = 0;
    fflush(stdout);
    char *c = __cz(cmd);
    FILE *p = popen(c, "r");
    if (!p) { __std_seterr("cannot run", cmd); free(c); return __pstr_lit("", 0); }
    size_t cap = 4096, len = 0, k;
    char *buf = (char *)malloc(cap);
    while ((k = fread(buf + len, 1, cap - len, p)) > 0) {
        len += k;
        if (len == cap) buf = (char *)realloc(buf, cap *= 2);
    }
    int st = pclose(p);
    int code = st == -1 ? -1 : WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
    if (code != 0) snprintf(__std_err, sizeof __std_err, "the command '%s' ended with exit code %d", c, code);
    free(c);
    PStr r = __pstr_mem(buf, (int64_t)len);
    free(buf);
    return r;
}
static PStr __std_readfile(PStr name) {
    __std_err[0] = 0;
    char *n = __cz(name);
    FILE *f = fopen(n, "rb");
    free(n);
    if (!f) { __std_seterr("cannot open", name); return __pstr_lit("", 0); }
    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    size_t k;
    while ((k = fread(buf + len, 1, cap - len, f)) > 0) {
        len += k;
        if (len == cap) buf = (char *)realloc(buf, cap *= 2);
    }
    if (ferror(f)) __std_seterr("cannot read", name);
    fclose(f);
    PStr r = __pstr_mem(buf, (int64_t)len);
    free(buf);
    return r;
}
static uint8_t __std_writefile(PStr name, PStr text, uint8_t append) {
    __std_err[0] = 0;
    char *n = __cz(name);
    FILE *f = fopen(n, append ? "ab" : "wb");
    free(n);
    if (!f) { __std_seterr("cannot open", name); return 0; }
    int ok = text.len == 0 || fwrite(text.p, 1, (size_t)text.len, f) == (size_t)text.len;
    if (fclose(f) != 0) ok = 0;
    if (!ok) __std_seterr("cannot write", name);
    return ok;
}
static uint8_t __std_fileexists(PStr name) { char *n = __cz(name); FILE *f = fopen(n, "rb"); free(n); if (f) fclose(f); return f != NULL; }
static uint8_t __std_deletefile(PStr name) {
    __std_err[0] = 0;
    char *n = __cz(name);
    int ok = remove(n) == 0;
    free(n);
    if (!ok) __std_seterr("cannot delete", name);
    return ok;
}
/* text files: a FILE* kept as Int64 in TTextFile */
static int64_t __std_fopen(PStr name, PStr mode) {
    __std_err[0] = 0;
    char *n = __cz(name), *m = __cz(mode);
    FILE *f = fopen(n, m);
    free(n);
    free(m);
    if (!f) __std_seterr("cannot open", name);
    return (int64_t)(intptr_t)f;
}
static uint8_t __std_freadline(int64_t h, PStr *line) {
    FILE *f = (FILE *)(intptr_t)h;
    if (!f) return 0;
    size_t cap = 256, len = 0;
    char *buf = (char *)malloc(cap);
    int c, any = 0;
    while ((c = fgetc(f)) != EOF) {
        any = 1;
        if (c == '\n') break;
        if (len + 1 >= cap) buf = (char *)realloc(buf, cap *= 2);
        buf[len++] = (char)c;
    }
    if (len && buf[len - 1] == '\r') len--;
    if (any) __pstr_assign(line, __pstr_lit(buf, (int64_t)len));
    free(buf);
    return any;
}
static uint8_t __std_fwrite(int64_t h, PStr s) {
    FILE *f = (FILE *)(intptr_t)h;
    return f && (s.len == 0 || fwrite(s.p, 1, (size_t)s.len, f) == (size_t)s.len);
}
static uint8_t __std_feof(int64_t h) {
    FILE *f = (FILE *)(intptr_t)h;
    if (!f) return 1;
    int c = fgetc(f);
    if (c == EOF) return 1;
    ungetc(c, f);
    return 0;
}
static void __std_fclose(int64_t h) { FILE *f = (FILE *)(intptr_t)h; if (f) fclose(f); }
/* directories */
static int64_t __std_dir_open(PStr path) {
    __std_err[0] = 0;
    char *n = __cz(path);
    DIR *d = opendir(n);
    free(n);
    if (!d) __std_seterr("cannot list", path);
    return (int64_t)(intptr_t)d;
}
static uint8_t __std_dir_next(int64_t h, PStr *name) { /* the next entry without '.' and '..' */
    DIR *d = (DIR *)(intptr_t)h;
    struct dirent *e;
    while (d && (e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        __pstr_assign(name, __pstr_lit(e->d_name, (int64_t)strlen(e->d_name)));
        return 1;
    }
    return 0;
}
static void __std_dir_close(int64_t h) { if (h) closedir((DIR *)(intptr_t)h); }
static uint8_t __std_direxists(PStr path) { char *n = __cz(path); struct stat st; int ok = stat(n, &st) == 0 && S_ISDIR(st.st_mode); free(n); return ok; }
static uint8_t __std_mkdir(PStr path) {
    __std_err[0] = 0;
    char *n = __cz(path);
    int ok = mkdir(n, 0777) == 0;
    free(n);
    if (!ok) __std_seterr("cannot create directory", path);
    return ok;
}
static uint8_t __std_rmdir(PStr path) {
    __std_err[0] = 0;
    char *n = __cz(path);
    int ok = rmdir(n) == 0;
    free(n);
    if (!ok) __std_seterr("cannot remove directory", path);
    return ok;
}
static uint8_t __std_rename(PStr from, PStr to) {
    __std_err[0] = 0;
    char *a = __cz(from), *b = __cz(to);
    int ok = rename(a, b) == 0;
    free(a);
    free(b);
    if (!ok) __std_seterr("cannot rename", from);
    return ok;
}
static int64_t __std_filesize(PStr path) { /* -1: no such file (the error is kept) */
    __std_err[0] = 0;
    char *n = __cz(path);
    struct stat st;
    int ok = stat(n, &st) == 0;
    free(n);
    if (!ok) { __std_seterr("cannot read the size of", path); return -1; }
    return (int64_t)st.st_size;
}
static PStr __std_getcwd(void) { char b[4096]; if (!getcwd(b, sizeof b)) return __pstr_lit("", 0); return __pstr_mem(b, (int64_t)strlen(b)); }
static uint8_t __std_chdir(PStr path) {
    __std_err[0] = 0;
    char *n = __cz(path);
    int ok = chdir(n) == 0;
    free(n);
    if (!ok) __std_seterr("cannot change to directory", path);
    return ok;
}
/* time */
static int64_t __std_milliseconds(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static int64_t __std_unixtime(void) { return (int64_t)time(NULL); }
static void __std_sleep(int32_t ms) { if (ms > 0) { struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000000L}; nanosleep(&t, NULL); } }
static void __std_now(int32_t *y, int32_t *mo, int32_t *d, int32_t *h, int32_t *mi, int32_t *s, int32_t *ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t t = ts.tv_sec;
    struct tm tm;
    localtime_r(&t, &tm);
    *y = tm.tm_year + 1900; *mo = tm.tm_mon + 1; *d = tm.tm_mday;
    *h = tm.tm_hour; *mi = tm.tm_min; *s = tm.tm_sec; *ms = (int32_t)(ts.tv_nsec / 1000000);
}
/* program parameters and environment */
static int32_t __std_paramcount(void) { return __ppc_argc - 1; }
static PStr __std_paramstr(int32_t i) { if (i < 0 || i >= __ppc_argc) return __pstr_lit("", 0); return __pstr_mem(__ppc_argv[i], (int64_t)strlen(__ppc_argv[i])); }
static PStr __std_getenv(PStr name) { char *n = __cz(name); const char *v = getenv(n); free(n); return v ? __pstr_mem(v, (int64_t)strlen(v)) : __pstr_lit("", 0); }
/* ---- end of runtime ---- */
