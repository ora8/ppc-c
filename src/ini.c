/* ini.c - ppc.ini: the project file of ppm (also read by ppc for the module paths of a project) */
#include "ini.h"
#include <ctype.h>
#include <limits.h>
#include <unistd.h>

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = xmalloc((size_t)n + 1);
    size_t got = fread(b, 1, (size_t)n, f);
    b[got] = 0;
    fclose(f);
    return b;
}

/* [section]  key = value  ; comment  # comment */
int ini_read(const char *path, Ini *ini, char **err) {
    char *src = read_all(path);
    if (!src) {
        *err = sfmt("cannot read '%s'", path);
        return 1;
    }
    ini->path = xstrdup(path);
    ini->entries.len = 0;
    char *section = xstrdup("");
    int line = 0;
    for (char *p = src; *p;) {
        char *e = strchr(p, '\n');
        if (e) *e = 0;
        line++;
        char *l = trim(p);
        p = e ? e + 1 : p + strlen(p);
        if (!*l || *l == ';' || *l == '#') continue;
        if (*l == '[') {
            char *c = strchr(l, ']');
            if (!c || (trim(c + 1)[0] && trim(c + 1)[0] != ';' && trim(c + 1)[0] != '#')) {
                *err = sfmt("%s(%d): a section is written '[name]'", path, line);
                return 1;
            }
            *c = 0;
            section = xstrdup(trim(l + 1));
            continue;
        }
        char *eq = strchr(l, '=');
        if (!eq) {
            *err = sfmt("%s(%d): 'key = value' expected, found '%s'", path, line, l);
            return 1;
        }
        *eq = 0;
        char *v = eq + 1;
        for (char *q = v; *q; q++) /* a comment after the value: ' ; ...' or ' # ...' */
            if ((*q == ';' || *q == '#') && (q == v || isspace((unsigned char)q[-1]))) {
                *q = 0;
                break;
            }
        IniEntry en = {section, xstrdup(trim(l)), xstrdup(trim(v)), line};
        if (!*en.key) {
            *err = sfmt("%s(%d): the key before '=' is missing", path, line);
            return 1;
        }
        for (int i = 0; i < ini->entries.len; i++)
            if (STREQ(ini->entries.items[i].section, en.section) && STREQ(ini->entries.items[i].key, en.key)) {
                *err = sfmt("%s(%d): '%s' is given twice in [%s] (first in line %d)", path, line, en.key, en.section,
                            ini->entries.items[i].line);
                return 1;
            }
        vpush(ini->entries, en);
    }
    return 0;
}

const char *ini_get(const Ini *ini, const char *section, const char *key) {
    for (int i = 0; i < ini->entries.len; i++)
        if (STREQ(ini->entries.items[i].section, section) && STREQ(ini->entries.items[i].key, key)) return ini->entries.items[i].value;
    return NULL;
}

StrVec ini_split(const char *value, const char *seps) {
    StrVec v = {0};
    if (!value) return v;
    char *s = xstrdup(value);
    for (char *t = s; *t;) {
        size_t n = strcspn(t, seps);
        char *part = xstrndup(t, n);
        char *tr = trim(part);
        if (*tr) vpush(v, xstrdup(tr));
        t += n + (t[n] ? 1 : 0);
    }
    return v;
}

static int exists(const char *p) { return access(p, F_OK) == 0; }

/* the first two words of a source file (comments skipped): 'program X', 'library X', 'module X' */
int source_header(const char *path, char **word, char **name) {
    char *s = read_all(path);
    if (!s) return 1;
    char *w[2] = {NULL, NULL};
    int n = 0;
    for (char *p = s; *p && n < 2;) {
        if (isspace((unsigned char)*p)) p++;
        else if (p[0] == '{' && p[1] == '*') { char *e = strstr(p + 2, "*}"); p = e ? e + 2 : p + strlen(p); }
        else if (p[0] == '(' && p[1] == '*') { char *e = strstr(p + 2, "*)"); p = e ? e + 2 : p + strlen(p); }
        else if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; }
        else if (isalpha((unsigned char)*p) || *p == '_') {
            char *b = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            w[n++] = xstrndup(b, (size_t)(p - b));
        } else break;
    }
    *word = w[0];
    *name = w[1];
    return 0;
}

int project_load(const char *ini_path, Project *p, char **err) {
    memset(p, 0, sizeof *p);
    if (ini_read(ini_path, &p->ini, err)) return 1;
    p->ini_path = xstrdup(ini_path);
    const char *sl = strrchr(ini_path, '/');
    p->dir = sl ? xstrndup(ini_path, (size_t)(sl - ini_path + 1)) : xstrdup("");
    const char *name = ini_get(&p->ini, "project", "name");
    if (!name || !*name) {
        *err = sfmt("%s: [project] has no 'name = ...'", ini_path);
        return 1;
    }
    p->name = xstrdup(name);
    const char *kind = ini_get(&p->ini, "project", "kind");
    if (!kind || STREQ(kind, "program")) p->is_library = 0;
    else if (STREQ(kind, "library")) p->is_library = 1;
    else if (STREQ(kind, "modules")) p->is_modules = 1;
    else {
        *err = sfmt("%s: kind = '%s': 'program', 'library' or 'modules' expected", ini_path, kind);
        return 1;
    }
    const char *st = ini_get(&p->ini, "project", "static");
    p->is_static = st && (STREQ(st, "yes") || STREQ(st, "true") || STREQ(st, "1"));
    if (st && !p->is_static && !(STREQ(st, "no") || STREQ(st, "false") || STREQ(st, "0"))) {
        *err = sfmt("%s: static = '%s': 'yes' or 'no' expected", ini_path, st);
        return 1;
    }
    if (p->is_static && !p->is_library) {
        *err = sfmt("%s: static = yes is only for a library (kind = library)", ini_path);
        return 1;
    }
    const char *m = ini_get(&p->ini, "project", "main");
    if (p->is_modules) {
        if (m) {
            *err = sfmt("%s: kind = modules has no main file", ini_path);
            return 1;
        }
    } else if (m) {
        p->main = m[0] == '/' ? xstrdup(m) : sfmt("%s%s", p->dir, m);
        if (!exists(p->main)) {
            *err = sfmt("%s: main = '%s' does not exist", ini_path, m);
            return 1;
        }
    } else {
        const char *base = p->is_library ? "lib" : "main";
        char *a = sfmt("%ssrc/%s.pas", p->dir, base), *b = sfmt("%ssrc/%s.pps", p->dir, base);
        p->main = exists(a) ? a : exists(b) ? b : NULL;
    }
    return 0;
}

static int add_dirs(const Project *p, StrVec *dirs, StrVec *seen, char **err) {
    char rp[PATH_MAX];
    if (!realpath(p->dir[0] ? p->dir : ".", rp)) {
        *err = sfmt("cannot find the directory of '%s'", p->ini_path);
        return 1;
    }
    for (int i = 0; i < seen->len; i++)
        if (STREQ(seen->items[i], rp)) return 0; /* already there (a shared dependency, or a cycle) */
    vpush(*seen, xstrdup(rp));
    char *src = sfmt("%ssrc/", p->dir);
    if (exists(src)) vpush(*dirs, src);
    for (int i = 0; i < p->ini.entries.len; i++) {
        IniEntry *e = &p->ini.entries.items[i];
        if (!STREQ(e->section, "dependencies")) continue;
        char *d = e->value[0] == '/' ? xstrdup(e->value) : sfmt("%s%s", p->dir, e->value);
        if (d[strlen(d) - 1] != '/') d = sfmt("%s/", d);
        char *ini = sfmt("%sppc.ini", d);
        if (!exists(ini)) {
            *err = sfmt("%s(%d): the dependency '%s' (%s) has no ppc.ini", p->ini_path, e->line, e->key, e->value);
            return 1;
        }
        Project dp;
        if (project_load(ini, &dp, err)) return 1;
        if (!STREQ(dp.name, e->key)) {
            *err = sfmt("%s(%d): the dependency '%s' is the project '%s' (%s)", p->ini_path, e->line, e->key, dp.name, ini);
            return 1;
        }
        if (add_dirs(&dp, dirs, seen, err)) return 1;
    }
    return 0;
}

int project_module_dirs(const Project *p, StrVec *dirs, char **err) {
    StrVec seen = {0};
    return add_dirs(p, dirs, &seen, err);
}

char *project_find(const char *dir) {
    char rp[PATH_MAX];
    if (!realpath(dir && *dir ? dir : ".", rp)) return NULL;
    for (;;) {
        char *f = sfmt("%s%sppc.ini", rp, STREQ(rp, "/") ? "" : "/");
        if (exists(f)) return f;
        char *s = strrchr(rp, '/');
        if (!s || s == rp) {
            if (STREQ(rp, "/")) return NULL;
            strcpy(rp, "/");
            continue;
        }
        *s = 0;
    }
}
