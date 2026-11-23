/* ---- ppc runtime (embedded into every generated program) ---- */
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { unsigned char s[256]; } PStr; /* ShortString: s[0] = length */
struct __VMTHdr { void *parent; const char *name; size_t size; };

static void __rt_error(int code, const char *msg) {
    fflush(stdout);
    fprintf(stderr, "Runtime error %d: %s\n", code, msg);
    exit(code);
}
static void *__ppc_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) __rt_error(203, "heap overflow");
    return p;
}
static void __ppc_free(void *p) { free(p); }
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
static const char *__ppc_srcfile;
static void *__nilchk(void *p, int line, int col) {
    if (!p) {
        fflush(stdout);
        fprintf(stderr, "Runtime error 210: nil pointer dereference at %s(%d,%d)\n", __ppc_srcfile, line, col);
        exit(210);
    }
    return p;
}

/* ---- strings ---- */
static PStr __pstr_lit(const char *c, int n) {
    PStr r;
    if (n > 255) n = 255;
    r.s[0] = (unsigned char)n;
    memcpy(r.s + 1, c, (size_t)n);
    return r;
}
static PStr __pstr_char(unsigned char c) { PStr r; r.s[0] = 1; r.s[1] = c; return r; }
static PStr __pstr_cstr(const char *c) { return __pstr_lit(c, (int)strlen(c)); }
static PStr __pstr_cat(PStr a, PStr b) {
    int n = a.s[0], m = b.s[0];
    if (n + m > 255) m = 255 - n;
    memcpy(a.s + 1 + n, b.s + 1, (size_t)m);
    a.s[0] = (unsigned char)(n + m);
    return a;
}
static int __pstr_cmp(PStr a, PStr b) {
    int n = a.s[0], m = b.s[0], k = n < m ? n : m;
    int c = memcmp(a.s + 1, b.s + 1, (size_t)k);
    if (c) return c;
    return n - m;
}
static PStr __pstr_copy(PStr s, int64_t i, int64_t c) {
    PStr r;
    int n = s.s[0];
    if (i < 1) i = 1;
    if (i > n || c <= 0) { r.s[0] = 0; return r; }
    if (c > n - i + 1) c = n - i + 1;
    r.s[0] = (unsigned char)c;
    memcpy(r.s + 1, s.s + i, (size_t)c);
    return r;
}
static int32_t __pstr_pos(PStr sub, PStr s) {
    int n = s.s[0], m = sub.s[0];
    if (m == 0) return 0;
    for (int i = 1; i + m - 1 <= n; i++)
        if (memcmp(s.s + i, sub.s + 1, (size_t)m) == 0) return i;
    return 0;
}
static void __pstr_delete(PStr *s, int64_t i, int64_t c) {
    int n = s->s[0];
    if (i < 1 || i > n || c <= 0) return;
    if (c > n - i + 1) c = n - i + 1;
    memmove(s->s + i, s->s + i + c, (size_t)(n - i - c + 1));
    s->s[0] = (unsigned char)(n - c);
}
static void __pstr_insert(PStr src, PStr *s, int64_t i) {
    int n = s->s[0];
    if (i < 1) i = 1;
    if (i > n + 1) i = n + 1;
    PStr head = __pstr_copy(*s, 1, i - 1), tail = __pstr_copy(*s, i, 255);
    *s = __pstr_cat(__pstr_cat(head, src), tail);
}
static PStr __int_to_str(int64_t v) { char b[32]; int n = snprintf(b, sizeof b, "%lld", (long long)v); return __pstr_lit(b, n); }
static int32_t __str_to_int(PStr s) {
    char b[257];
    memcpy(b, s.s + 1, s.s[0]);
    b[s.s[0]] = 0;
    char *e;
    long long v = strtoll(b, &e, 10);
    if (s.s[0] == 0 || *e) {
        fflush(stdout);
        fprintf(stderr, "EConvertError: \"%s\" is an invalid integer\n", b);
        exit(1);
    }
    return (int32_t)v;
}
static PStr __float_to_str(double d) {
    char b[64];
    int n = snprintf(b, sizeof b, "%.15g", d);
    return __pstr_lit(b, n);
}
static PStr __upper(PStr s) { for (int i = 1; i <= s.s[0]; i++) s.s[i] = (unsigned char)toupper(s.s[i]); return s; }
static PStr __lower(PStr s) { for (int i = 1; i <= s.s[0]; i++) s.s[i] = (unsigned char)tolower(s.s[i]); return s; }
static unsigned char __upcase(unsigned char c) { return (unsigned char)toupper(c); }

/* ---- output ---- */
static void __w_pad(int64_t w, int len) { for (; len < w; len++) putchar(' '); }
static void __w_str(PStr s, int64_t w) { __w_pad(w, s.s[0]); fwrite(s.s + 1, 1, s.s[0], stdout); }
static void __w_cstr(const char *s, int64_t w) { __w_pad(w, (int)strlen(s)); fputs(s, stdout); }
static void __w_char(unsigned char c, int64_t w) { __w_pad(w, 1); putchar(c); }
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
static PStr __str_real(double v, int64_t w, int64_t p) { char b[128]; __fmt_real(b, sizeof b, v, w, p); return __pstr_cstr(b); }
static PStr __str_int(int64_t v, int64_t w) { char b[64]; snprintf(b, sizeof b, "%*lld", (int)w, (long long)v); return __pstr_cstr(b); }

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
static PStr __r_str(void) {
    PStr r;
    int n = 0, c;
    fflush(stdout);
    while ((c = getchar()) != EOF && c != '\n') {
        if (c == '\r') continue;
        if (n < 255) r.s[1 + n++] = (unsigned char)c;
    }
    if (c == '\n') ungetc(c, stdin);
    r.s[0] = (unsigned char)n;
    return r;
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
/* ---- end of runtime ---- */
