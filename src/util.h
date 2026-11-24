/* util.h - memory, strings, vectors, diagnostics */
#ifndef PPC_UTIL_H
#define PPC_UTIL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int line, col;
} Pos;

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t sz);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
char *sfmt(const char *fmt, ...);

#define NEW(T) ((T *)xcalloc(1, sizeof(T)))
#define STREQ(a, b) (strcmp((a), (b)) == 0)

/* growable vector */
#define VEC(T)        \
    struct {          \
        T *items;     \
        int len, cap; \
    }
#define vpush(v, x)                                                              \
    do {                                                                         \
        if ((v).len >= (v).cap) {                                                \
            (v).cap = (v).cap ? (v).cap * 2 : 8;                                 \
            (v).items = xrealloc((v).items, (size_t)(v).cap * sizeof(*(v).items)); \
        }                                                                        \
        (v).items[(v).len++] = (x);                                              \
    } while (0)

/* string buffer */
typedef struct {
    char *d;
    size_t len, cap;
} Buf;
void buf_put(Buf *b, const char *s);
void buf_putn(Buf *b, const char *s, size_t n);
void buf_printf(Buf *b, const char *fmt, ...);
const char *buf_str(Buf *b);

/* diagnostics */
extern const char *g_filename;
extern int g_warnings;
extern const char *g_err_context; /* e.g. the generic instance being checked */
#include <setjmp.h>
extern jmp_buf *g_recover; /* error recovery point: error_at jumps back here instead of stopping */
extern int g_errors;
#define MAX_ERRORS 20
_Noreturn void error_at(Pos p, const char *fmt, ...);
void warning_at(Pos p, const char *fmt, ...);
void note_at(Pos p, const char *fmt, ...);
void verror_at(const char *kind, Pos p, const char *fmt, va_list ap);

#endif
