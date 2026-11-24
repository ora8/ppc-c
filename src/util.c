/* util.c */
#include "util.h"

const char *g_filename = "<input>";
int g_warnings = 0;

void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "ppc: out of memory\n");
        exit(2);
    }
    return p;
}
void *xcalloc(size_t n, size_t sz) {
    void *p = calloc(n ? n : 1, sz ? sz : 1);
    if (!p) {
        fprintf(stderr, "ppc: out of memory\n");
        exit(2);
    }
    return p;
}
void *xrealloc(void *p, size_t n) {
    p = realloc(p, n ? n : 1);
    if (!p) {
        fprintf(stderr, "ppc: out of memory\n");
        exit(2);
    }
    return p;
}
char *xstrdup(const char *s) { return xstrndup(s, strlen(s)); }
char *xstrndup(const char *s, size_t n) {
    char *r = xmalloc(n + 1);
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}
char *sfmt(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *r = xmalloc((size_t)n + 1);
    vsnprintf(r, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return r;
}

void buf_putn(Buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap : 256;
        while (c < b->len + n + 1) c *= 2;
        b->d = xrealloc(b->d, c);
        b->cap = c;
    }
    memcpy(b->d + b->len, s, n);
    b->len += n;
    b->d[b->len] = 0;
}
void buf_put(Buf *b, const char *s) { buf_putn(b, s, strlen(s)); }
void buf_printf(Buf *b, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *tmp = xmalloc((size_t)n + 1);
    vsnprintf(tmp, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    buf_putn(b, tmp, (size_t)n);
    free(tmp);
}
const char *buf_str(Buf *b) { return b->d ? b->d : ""; }

void verror_at(const char *kind, Pos p, const char *fmt, va_list ap) {
    fprintf(stderr, "%s(%d,%d) %s: ", POS_FILE(p), p.line, p.col, kind);
    vfprintf(stderr, fmt, ap);
    if (g_err_context) fprintf(stderr, " (in %s)", g_err_context);
    fputc('\n', stderr);
}
const char *g_err_context;
jmp_buf *g_recover;
int g_errors;
_Noreturn void error_at(Pos p, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at("Error", p, fmt, ap);
    va_end(ap);
    if (++g_errors >= MAX_ERRORS) {
        fprintf(stderr, "%s: too many errors (%d), stopping\n", g_filename, g_errors);
        exit(1);
    }
    if (g_recover) longjmp(*g_recover, 1); /* report the next errors too */
    exit(1);
}
void warning_at(Pos p, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at("Warning", p, fmt, ap);
    va_end(ap);
    g_warnings++;
}
void note_at(Pos p, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at("Note", p, fmt, ap);
    va_end(ap);
}
