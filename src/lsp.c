/* lsp.c - a language server for Perfect Pascal (LSP over standard input/output)
 *
 * The editor sends the text of each open .pas file; after every change the server runs
 * 'ppc --check --stdin file.pas' in a child process (the compiler keeps global state and stops
 * after too many errors, so a fresh process per check is the simplest robust way) and sends
 * its messages back as diagnostics. Modules next to the file are found as usual. */
#include "util.h"
#include <ctype.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

/* ------------------------------------------------------------------ JSON */

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JKind;
typedef struct Json Json;
struct Json {
    JKind kind;
    double num;
    char *str;     /* J_STR; also the key of an object member */
    VEC(Json *) items; /* J_ARR elements, J_OBJ members (each with str = key) */
    Json *val;     /* object member: its value */
    int truth;
};

static const char *jp;
static void jws(void) {
    while (*jp == ' ' || *jp == '\t' || *jp == '\n' || *jp == '\r') jp++;
}
static Json *jnew(JKind k) {
    Json *j = xcalloc(1, sizeof(Json));
    j->kind = k;
    return j;
}
static void jutf8(Buf *b, unsigned cp) {
    char t[4];
    if (cp < 0x80) { t[0] = (char)cp; buf_putn(b, t, 1); }
    else if (cp < 0x800) { t[0] = (char)(0xC0 | (cp >> 6)); t[1] = (char)(0x80 | (cp & 63)); buf_putn(b, t, 2); }
    else { t[0] = (char)(0xE0 | (cp >> 12)); t[1] = (char)(0x80 | ((cp >> 6) & 63)); t[2] = (char)(0x80 | (cp & 63)); buf_putn(b, t, 3); }
}
static char *jstring(void) {
    Buf b = {0};
    jp++; /* " */
    while (*jp && *jp != '"') {
        if (*jp == '\\') {
            jp++;
            char c = *jp++;
            switch (c) {
            case 'n': buf_put(&b, "\n"); break;
            case 't': buf_put(&b, "\t"); break;
            case 'r': buf_put(&b, "\r"); break;
            case 'b': buf_put(&b, "\b"); break;
            case 'f': buf_put(&b, "\f"); break;
            case 'u': {
                unsigned cp = 0;
                for (int i = 0; i < 4 && *jp; i++, jp++) cp = cp * 16 + (unsigned)(isdigit((unsigned char)*jp) ? *jp - '0' : (tolower((unsigned char)*jp) - 'a' + 10));
                jutf8(&b, cp);
                break;
            }
            default: buf_putn(&b, &c, 1); break;
            }
        } else
            buf_putn(&b, jp++, 1);
    }
    if (*jp == '"') jp++;
    return xstrdup(buf_str(&b));
}
static Json *jvalue(void) {
    jws();
    if (*jp == '{') {
        Json *o = jnew(J_OBJ);
        jp++;
        jws();
        while (*jp && *jp != '}') {
            jws();
            Json *m = jnew(J_NULL);
            m->str = jstring();
            jws();
            if (*jp == ':') jp++;
            m->val = jvalue();
            vpush(o->items, m);
            jws();
            if (*jp == ',') jp++;
            jws();
        }
        if (*jp == '}') jp++;
        return o;
    }
    if (*jp == '[') {
        Json *a = jnew(J_ARR);
        jp++;
        jws();
        while (*jp && *jp != ']') {
            vpush(a->items, jvalue());
            jws();
            if (*jp == ',') jp++;
            jws();
        }
        if (*jp == ']') jp++;
        return a;
    }
    if (*jp == '"') {
        Json *s = jnew(J_STR);
        s->str = jstring();
        return s;
    }
    if (!strncmp(jp, "true", 4)) { jp += 4; Json *b = jnew(J_BOOL); b->truth = 1; return b; }
    if (!strncmp(jp, "false", 5)) { jp += 5; return jnew(J_BOOL); }
    if (!strncmp(jp, "null", 4)) { jp += 4; return jnew(J_NULL); }
    Json *n = jnew(J_NUM);
    char *end;
    n->num = strtod(jp, &end);
    n->str = xstrndup(jp, (size_t)(end - jp)); /* the number as written (ids are echoed back) */
    jp = end > jp ? end : jp + 1;
    return n;
}
static Json *jget(Json *o, const char *key) {
    if (!o || o->kind != J_OBJ) return NULL;
    for (int i = 0; i < o->items.len; i++)
        if (STREQ(o->items.items[i]->str, key)) return o->items.items[i]->val;
    return NULL;
}
static const char *jstr(Json *j) { return j && j->kind == J_STR ? j->str : NULL; }
static int utf8_len(const unsigned char *s) { /* length of a valid UTF-8 sequence at s, 0 if invalid */
    int n = s[0] >= 0xF0 ? 4 : s[0] >= 0xE0 ? 3 : s[0] >= 0xC2 ? 2 : 0;
    for (int i = 1; i < n; i++)
        if ((s[i] & 0xC0) != 0x80) return 0;
    return n;
}
static void jesc(Buf *b, const char *s) { /* a JSON string literal (invalid UTF-8 becomes '?') */
    buf_put(b, "\"");
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= 0x80) {
            int n = utf8_len((const unsigned char *)s);
            if (n) {
                buf_putn(b, s, (size_t)n);
                s += n - 1;
            } else
                buf_put(b, "?");
            continue;
        }
        if (c == '"') buf_put(b, "\\\"");
        else if (c == '\\') buf_put(b, "\\\\");
        else if (c == '\n') buf_put(b, "\\n");
        else if (c == '\r') buf_put(b, "\\r");
        else if (c == '\t') buf_put(b, "\\t");
        else if (c < 0x20) buf_printf(b, "\\u%04x", c);
        else buf_putn(b, s, 1);
    }
    buf_put(b, "\"");
}
static void jid(Buf *b, Json *id) { /* the request id, echoed as it came */
    if (!id) buf_put(b, "null");
    else if (id->kind == J_STR) jesc(b, id->str);
    else if (id->kind == J_NUM) buf_put(b, id->str);
    else buf_put(b, "null");
}

/* ------------------------------------------------------------- transport */

static void send_msg(const char *body) {
    fprintf(stdout, "Content-Length: %zu\r\n\r\n%s", strlen(body), body);
    fflush(stdout);
}
static char *read_msg(void) { /* NULL at the end of the input */
    long len = -1;
    char line[1024];
    for (;;) {
        if (!fgets(line, sizeof line, stdin)) return NULL;
        if (!strncasecmp(line, "Content-Length:", 15)) len = strtol(line + 15, NULL, 10);
        if (!strcmp(line, "\r\n") || !strcmp(line, "\n")) break;
    }
    if (len < 0) return xstrdup("{}");
    char *b = xmalloc((size_t)len + 1);
    size_t got = fread(b, 1, (size_t)len, stdin);
    b[got] = 0;
    return b;
}

/* ----------------------------------------------------------- documents */

typedef struct {
    char *uri, *path, *text;
} Doc;
static VEC(Doc) docs;
static char *self_exe;

static char *uri_to_path(const char *uri) {
    const char *p = uri;
    if (!strncmp(p, "file://", 7)) p += 7;
    Buf b = {0};
    for (; *p; p++) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char h[3] = {p[1], p[2], 0};
            char c = (char)strtol(h, NULL, 16);
            buf_putn(&b, &c, 1);
            p += 2;
        } else
            buf_putn(&b, p, 1);
    }
    return xstrdup(buf_str(&b));
}
static Doc *doc_of(const char *uri) {
    for (int i = 0; i < docs.len; i++)
        if (STREQ(docs.items[i].uri, uri)) return &docs.items[i];
    Doc d = {xstrdup(uri), uri_to_path(uri), xstrdup("")};
    vpush(docs, d);
    return &docs.items[docs.len - 1];
}

/* run 'ppc --check --stdin path' (or with '--hover L:C') on the text; returns its stderr (or its stdout for hover) */
static char *run_ppc(Doc *d, const char *hover);
static const char *query_opt = "--hover"; /* or --definition */
static const char *run_text;              /* the text to check instead of the document's (completion) */
static char *run_check(Doc *d) { return run_ppc(d, NULL); }
static char *run_ppc(Doc *d, const char *hover) {
    int in[2], err[2];
    if (pipe(in) || pipe(err)) return xstrdup("");
    pid_t pid = fork();
    if (pid == 0) {
        dup2(in[0], 0);
        int devnull = open("/dev/null", 1);
        dup2(err[1], hover ? 1 : 2); /* the messages, or the description of --hover */
        if (devnull >= 0) dup2(devnull, hover ? 2 : 1);
        close(in[1]);
        close(err[0]);
        if (hover && STREQ(hover, "complete")) execl(self_exe, self_exe, "--check", "--stdin", "--complete", d->path, (char *)NULL);
        else if (hover) execl(self_exe, self_exe, "--check", "--stdin", query_opt, hover, d->path, (char *)NULL);
        else execl(self_exe, self_exe, "--check", "--stdin", d->path, (char *)NULL);
        _exit(127);
    }
    close(in[0]);
    close(err[1]);
    /* write the text, then read the messages (small programs fit the pipe; write in a child for large ones) */
    pid_t wpid = fork();
    if (wpid == 0) {
        close(err[0]);
        const char *txt = run_text ? run_text : d->text;
        size_t n = strlen(txt), off = 0;
        while (off < n) {
            ssize_t k = write(in[1], txt + off, n - off);
            if (k <= 0) break;
            off += (size_t)k;
        }
        _exit(0);
    }
    close(in[1]);
    Buf out = {0};
    char tmp[4096];
    ssize_t k;
    while ((k = read(err[0], tmp, sizeof tmp)) > 0) buf_putn(&out, tmp, (size_t)k);
    close(err[0]);
    waitpid(wpid, NULL, 0);
    waitpid(pid, NULL, 0);
    return xstrdup(buf_str(&out));
}

static const char *base_name(const char *p) {
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

/* the compiler counts columns in bytes, editors in UTF-16 units: convert for the line in the text */
static int utf16_col(const char *text, int line, int bytecol) {
    const char *p = text;
    for (int l = 1; l < line && *p; p++)
        if (*p == '\n') l++;
    int units = 0;
    for (int i = 1; i < bytecol && *p && *p != '\n'; i++, p++) {
        unsigned char c = (unsigned char)*p;
        if ((c & 0xC0) == 0x80) continue;    /* continuation byte */
        units += (c >= 0xF0) ? 2 : 1;        /* 4-byte characters are two UTF-16 units */
    }
    return units;
}

/* the other way: an editor column (UTF-16 units) as the compiler's 1-based byte column */
static int byte_col(const char *text, int line, int units) {
    const char *p = text;
    for (int l = 1; l < line && *p; p++)
        if (*p == '\n') l++;
    const char *start = p;
    int u = 0;
    while (*p && *p != '\n' && u < units) {
        unsigned char c = (unsigned char)*p;
        int len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        u += c >= 0xF0 ? 2 : 1;
        p += len;
    }
    return (int)(p - start) + 1;
}

/* 'file(line,col) Kind: message' lines of the compiler as LSP diagnostics */
static void publish(Doc *d) {
    char *out = run_check(d);
    Buf b = {0};
    buf_put(&b, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
    jesc(&b, d->uri);
    buf_put(&b, ",\"diagnostics\":[");
    int n = 0;
    for (char *line = out; *line;) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char *paren = strchr(line, '(');
        int ln = 0, col = 0, used = 0;
        if (paren && sscanf(paren, "(%d,%d)%n", &ln, &col, &used) == 2 && paren[used] == ' ') {
            char *file = xstrndup(line, (size_t)(paren - line));
            char *kind = paren + used + 1;
            char *colon = strstr(kind, ": ");
            if (colon) {
                char *k = xstrndup(kind, (size_t)(colon - kind));
                const char *msg = colon + 2;
                int sev = strstr(k, "Warning") ? 2 : strstr(k, "Note") ? 3 : 1;
                int here = STREQ(base_name(file), base_name(d->path));
                if (!here) { /* a message in an imported module: shown at the top of this file */
                    msg = sfmt("%s(%d,%d): %s", file, ln, col, msg);
                    ln = col = 1;
                }
                if (!STREQ(k, "Error") && !STREQ(k, "Warning") && !STREQ(k, "Note")) msg = sfmt("%s: %s", k, msg);
                int c0 = here ? utf16_col(d->text, ln, col) : 0;
                buf_printf(&b, "%s{\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}},"
                               "\"severity\":%d,\"source\":\"ppc\",\"message\":",
                           n++ ? "," : "", ln - 1, c0, ln - 1, c0 + 1, sev);
                jesc(&b, msg);
                buf_put(&b, "}");
            }
        }
        if (!nl) break;
        line = nl + 1;
    }
    buf_put(&b, "]}}");
    send_msg(buf_str(&b));
}

static void clear(const char *uri) {
    Buf b = {0};
    buf_put(&b, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
    jesc(&b, uri);
    buf_put(&b, ",\"diagnostics\":[]}}");
    send_msg(buf_str(&b));
}

/* completion: the marker in place of the word at the cursor; if the line is unfinished (it usually is while
   typing), also with the rest of the line cut off and the open brackets closed */
static const char *const MARK = "__ppc_complete__";
static int item_kind(const char *k) {
    static const struct { const char *k; int n; } K[] = {{"method", 2}, {"function", 3}, {"constructor", 4}, {"field", 5},
        {"variable", 6}, {"class", 7}, {"interface", 8}, {"property", 10}, {"enum", 13}, {"keyword", 14}, {"enummember", 20},
        {"constant", 21}, {"struct", 22}, {"type", 7}, {NULL, 0}};
    for (int i = 0; K[i].k; i++)
        if (STREQ(K[i].k, k)) return K[i].n;
    return 1;
}
static void complete(Buf *b, Doc *d, int line, int units) {
    const char *t = d->text, *p = t;
    for (int l = 1; l < line && *p; p++)
        if (*p == '\n') l++;
    const char *ls = p; /* the line */
    const char *le = strchr(ls, '\n');
    if (!le) le = ls + strlen(ls);
    const char *cur = ls + byte_col(t, line, units) - 1;
    if (cur > le) cur = le;
    const char *ws = cur; /* the word being typed */
    while (ws > ls && (isalnum((unsigned char)ws[-1]) || ws[-1] == '_')) ws--;
    char *prefix = xstrndup(ws, (size_t)(cur - ws));
    int parens = 0, brackets = 0;
    for (const char *q = ls; q < ws; q++) {
        if (*q == '\'') { /* skip strings */
            for (q++; q < ws && *q != '\''; q++) {}
            continue;
        }
        parens += *q == '(' ? 1 : *q == ')' ? -1 : 0;
        brackets += *q == '[' ? 1 : *q == ']' ? -1 : 0;
    }
    Buf closers = {0};
    for (int i = 0; i < brackets; i++) buf_put(&closers, "]");
    for (int i = 0; i < parens; i++) buf_put(&closers, ")");
    const char *variants[3];
    variants[0] = sfmt("%.*s%s%s", (int)(ws - t), t, MARK, cur);                                  /* as it is */
    variants[1] = sfmt("%.*s%s%s%s", (int)(ws - t), t, MARK, buf_str(&closers), le);             /* rest of line cut */
    variants[2] = sfmt("%.*s%s%s;%s", (int)(ws - t), t, MARK, buf_str(&closers), le);            /* ... and a ';' */
    char *out = NULL;
    for (int v = 0; v < 3 && (!out || !*out); v++) {
        run_text = variants[v];
        out = run_ppc(d, "complete");
        run_text = NULL;
    }
    int n = 0;
    for (char *l = out; l && *l;) {
        char *nl = strchr(l, '\n');
        if (nl) *nl = 0;
        char *t1 = strchr(l, '\t');
        char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        if (t1 && t2) {
            *t1 = 0;
            *t2 = 0;
            const char *name = t1 + 1;
            if (!strncasecmp(name, prefix, strlen(prefix))) {
                buf_printf(b, "%s{\"label\":", n++ ? "," : "");
                jesc(b, name);
                buf_printf(b, ",\"kind\":%d,\"detail\":", item_kind(l));
                jesc(b, t2 + 1);
                buf_put(b, "}");
            }
        }
        if (!nl) break;
        l = nl + 1;
    }
}

int lsp_main(const char *self) {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    self_exe = n > 0 ? xstrndup(buf, (size_t)n) : xstrdup(self);
    int shutting_down = 0;
    for (;;) {
        char *raw = read_msg();
        if (!raw) return shutting_down ? 0 : 1;
        jp = raw;
        Json *m = jvalue();
        const char *method = jstr(jget(m, "method"));
        Json *id = jget(m, "id");
        Json *params = jget(m, "params");
        if (!method) continue; /* a response to us: nothing to do */
        if (STREQ(method, "initialize")) {
            Buf b = {0};
            buf_put(&b, "{\"jsonrpc\":\"2.0\",\"id\":");
            jid(&b, id);
            buf_put(&b, ",\"result\":{\"capabilities\":{\"textDocumentSync\":{\"openClose\":true,\"change\":1,\"save\":{\"includeText\":false}},\"hoverProvider\":true,\"definitionProvider\":true,\"completionProvider\":{\"triggerCharacters\":[\".\"]}},"
                        "\"serverInfo\":{\"name\":\"ppc\",\"version\":\"1\"}}}");
            send_msg(buf_str(&b));
        } else if (STREQ(method, "shutdown")) {
            shutting_down = 1;
            Buf b = {0};
            buf_put(&b, "{\"jsonrpc\":\"2.0\",\"id\":");
            jid(&b, id);
            buf_put(&b, ",\"result\":null}");
            send_msg(buf_str(&b));
        } else if (STREQ(method, "exit")) {
            return shutting_down ? 0 : 1;
        } else if (STREQ(method, "textDocument/didOpen") || STREQ(method, "textDocument/didChange") ||
                   STREQ(method, "textDocument/didSave")) {
            Json *td = jget(params, "textDocument");
            const char *uri = jstr(jget(td, "uri"));
            if (!uri) continue;
            Doc *d = doc_of(uri);
            const char *text = jstr(jget(td, "text")); /* didOpen */
            Json *changes = jget(params, "contentChanges"); /* didChange: the whole text (full sync) */
            if (changes && changes->kind == J_ARR && changes->items.len)
                text = jstr(jget(changes->items.items[changes->items.len - 1], "text"));
            if (text) d->text = xstrdup(text);
            publish(d);
        } else if (STREQ(method, "textDocument/completion")) {
            const char *uri = jstr(jget(jget(params, "textDocument"), "uri"));
            Json *pos = jget(params, "position");
            Json *ln = jget(pos, "line"), *ch = jget(pos, "character");
            Buf b = {0};
            buf_put(&b, "{\"jsonrpc\":\"2.0\",\"id\":");
            jid(&b, id);
            buf_put(&b, ",\"result\":{\"isIncomplete\":false,\"items\":[");
            if (uri && ln && ch) complete(&b, doc_of(uri), (int)ln->num + 1, (int)ch->num);
            buf_put(&b, "]}}");
            send_msg(buf_str(&b));
        } else if (STREQ(method, "textDocument/definition")) {
            const char *uri = jstr(jget(jget(params, "textDocument"), "uri"));
            Json *pos = jget(params, "position");
            Json *ln = jget(pos, "line"), *ch = jget(pos, "character");
            Buf b = {0};
            buf_put(&b, "{\"jsonrpc\":\"2.0\",\"id\":");
            jid(&b, id);
            char *out = NULL;
            if (uri && ln && ch) {
                Doc *d = doc_of(uri);
                int line = (int)ln->num + 1;
                query_opt = "--definition";
                out = run_ppc(d, sfmt("%d:%d", line, byte_col(d->text, line, (int)ch->num)));
                query_opt = "--hover";
            }
            /* 'file:line:col' of the declaration (the file name may contain ':') */
            char *c2 = out ? strrchr(out, ':') : NULL, *c1 = NULL;
            if (c2) {
                *c2 = 0;
                c1 = strrchr(out, ':');
            }
            int dl = 0, dc = 0;
            if (c1 && sscanf(c1 + 1, "%d", &dl) == 1 && sscanf(c2 + 1, "%d", &dc) == 1) {
                *c1 = 0;
                char *file = out;
                if (file[0] != '/') { /* relative to the document's directory */
                    Doc *d = doc_of(uri);
                    const char *sl = strrchr(d->path, '/');
                    file = sl ? sfmt("%.*s/%s", (int)(sl - d->path), d->path, file) : file;
                }
                const char *text = NULL; /* the file's text: for the column in UTF-16 units */
                for (int i = 0; i < docs.len; i++)
                    if (STREQ(docs.items[i].path, file)) text = docs.items[i].text;
                FILE *f = text ? NULL : fopen(file, "rb");
                Buf ft = {0};
                if (f) {
                    char tmp[65536];
                    size_t k;
                    while ((k = fread(tmp, 1, sizeof tmp, f)) > 0) buf_putn(&ft, tmp, k);
                    fclose(f);
                    text = buf_str(&ft);
                }
                if (text) { /* the declaration is in a file (not in the built-in library) */
                    int c0 = utf16_col(text, dl, dc);
                    Buf u = {0};
                    buf_put(&u, "file://");
                    for (const char *q = file; *q; q++) {
                        if (isalnum((unsigned char)*q) || strchr("/-_.~", *q)) buf_putn(&u, q, 1);
                        else buf_printf(&u, "%%%02X", (unsigned char)*q);
                    }
                    buf_put(&b, ",\"result\":{\"uri\":");
                    jesc(&b, buf_str(&u));
                    buf_printf(&b, ",\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}}}",
                               dl - 1, c0, dl - 1, c0);
                    send_msg(buf_str(&b));
                    continue;
                }
            }
            buf_put(&b, ",\"result\":null}");
            send_msg(buf_str(&b));
        } else if (STREQ(method, "textDocument/hover")) {
            const char *uri = jstr(jget(jget(params, "textDocument"), "uri"));
            Json *pos = jget(params, "position");
            Json *ln = jget(pos, "line"), *ch = jget(pos, "character");
            Buf b = {0};
            buf_put(&b, "{\"jsonrpc\":\"2.0\",\"id\":");
            jid(&b, id);
            char *text = NULL;
            if (uri && ln && ch) {
                Doc *d = doc_of(uri);
                int line = (int)ln->num + 1;
                text = run_ppc(d, sfmt("%d:%d", line, byte_col(d->text, line, (int)ch->num)));
                size_t n = strlen(text);
                while (n && (text[n - 1] == '\n' || text[n - 1] == '\r')) text[--n] = 0;
            }
            if (text && *text) {
                buf_put(&b, ",\"result\":{\"contents\":{\"kind\":\"markdown\",\"value\":");
                jesc(&b, sfmt("```pascal\n%s\n```", text));
                buf_put(&b, "}}}");
            } else
                buf_put(&b, ",\"result\":null}");
            send_msg(buf_str(&b));
        } else if (STREQ(method, "textDocument/didClose")) {
            const char *uri = jstr(jget(jget(params, "textDocument"), "uri"));
            if (uri) clear(uri);
        } else if (id) { /* a request we do not support */
            Buf b = {0};
            buf_put(&b, "{\"jsonrpc\":\"2.0\",\"id\":");
            jid(&b, id);
            buf_put(&b, ",\"error\":{\"code\":-32601,\"message\":\"method not supported\"}}");
            send_msg(buf_str(&b));
        }
    }
}
