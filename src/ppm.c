/* ppm - Perfect Pascal Manager: projects with a ppc.ini (like cargo for Rust)

     ppm new <name> [--lib] [--static] [--pps]   a new project in the directory <name>
     ppm build                                   builds into target/
     ppm run [-- args]                           builds and runs the program
     ppm check                                   checks the program (library) and every module in src/
     ppm test                                    builds and runs the programs in tests/, compares with .expected
     ppm clean                                   removes target/

   The project is the ppc.ini in the current directory or above. ppm calls ppc (next to ppm, or $PPC):
   the src/ directories of the project and its dependencies become --module-paths, [link] becomes
   --shared-objects / --static-libraries, [build] options are passed on. */
#define _GNU_SOURCE /* nftw */
#include "ini.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *ppc_exe;
static Project proj;
static StrVec module_dirs;

static _Noreturn void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "ppm: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

static int exists(const char *p) { return access(p, F_OK) == 0; }

/* run a command (in dir, if not NULL); its exit status */
static int run(StrVec *av, const char *dir) {
    vpush(*av, NULL);
    av->len--;
    pid_t pid = fork();
    if (pid == 0) {
        if (dir && chdir(dir) != 0) _exit(126);
        execvp(av->items[0], av->items);
        fprintf(stderr, "ppm: cannot run '%s'\n", av->items[0]);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 128;
}

static void find_ppc(const char *argv0) {
    const char *env = getenv("PPC");
    if (env && *env) {
        ppc_exe = env;
        return;
    }
    char self[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);
    if (n > 0) {
        self[n] = 0;
        char *s = strrchr(self, '/');
        if (s) {
            char *p = sfmt("%.*s/ppc", (int)(s - self), self);
            if (access(p, X_OK) == 0) {
                ppc_exe = p;
                return;
            }
        }
    }
    (void)argv0;
    ppc_exe = "ppc";
}

static void mkdirs(const char *path) {
    char *p = xstrdup(path);
    for (char *s = p + 1; *s; s++)
        if (*s == '/') {
            *s = 0;
            mkdir(p, 0777);
            *s = '/';
        }
    if (mkdir(p, 0777) != 0 && errno != EEXIST) die("cannot create the directory '%s'", path);
}

static void write_file(const char *path, const char *text) {
    if (exists(path)) die("'%s' exists already", path);
    FILE *f = fopen(path, "w");
    if (!f) die("cannot write '%s'", path);
    fputs(text, f);
    fclose(f);
}

/* ---- ppm new ---- */
static int cmd_new(int argc, char **argv) {
    const char *name = NULL;
    int lib = 0, stat = 0, pps = 0;
    for (int i = 0; i < argc; i++) {
        if (STREQ(argv[i], "--lib")) lib = 1;
        else if (STREQ(argv[i], "--static")) stat = lib = 1;
        else if (STREQ(argv[i], "--pps")) pps = 1;
        else if (argv[i][0] == '-') die("unknown option '%s' for 'ppm new' (--lib, --static, --pps)", argv[i]);
        else if (!name) name = argv[i];
        else die("'ppm new' takes one name");
    }
    if (!name) die("usage: ppm new <name> [--lib] [--static] [--pps]");
    const char *base = strrchr(name, '/') ? strrchr(name, '/') + 1 : name;
    if (!*base || !(isalpha((unsigned char)*base) || *base == '_')) die("'%s': a project name starts with a letter", base);
    for (const char *c = base; *c; c++)
        if (!(isalnum((unsigned char)*c) || *c == '_' || *c == '-')) die("'%s': a project name has letters, digits, '_' and '-'", base);
    char *id = xstrdup(base); /* the Pascal name: '-' becomes '_' and the first letter is upper case */
    for (char *c = id; *c; c++)
        if (*c == '-') *c = '_';
    id[0] = (char)toupper((unsigned char)id[0]);
    if (exists(name)) die("'%s' exists already", name);
    mkdirs(sfmt("%s/src", name));
    mkdirs(sfmt("%s/tests", name));
    write_file(sfmt("%s/ppc.ini", name),
               sfmt("[project]\nname = %s\nversion = 0.1.0\nkind = %s\n%s\n[dependencies]\n; shapes = ../shapes\n\n"
                    "[link]\n; shared-objects = libz.so\n; static-libraries =\n\n[build]\noptions = -O2\n",
                    base, lib ? "library" : "program", stat ? "static = yes\n" : ""));
    write_file(sfmt("%s/.gitignore", name), "target/\n");
    /* a module of the project, used by the main file and by the test */
    if (pps)
        write_file(sfmt("%s/src/Greeting.pps", name),
                   "module Greeting;\ninterface\nfunction Hello(name: String): String;\nimplementation\n"
                   "function Hello(name: String): String; { Result := 'Hello, ' + name + '!'; }\n");
    else
        write_file(sfmt("%s/src/Greeting.pas", name),
                   "module Greeting;\ninterface\nfunction Hello(name: String): String;\nimplementation\n"
                   "function Hello(name: String): String;\nbegin\n  Result := 'Hello, ' + name + '!';\nend;\nend.\n");
    if (lib && pps)
        write_file(sfmt("%s/src/lib.pps", name),
                   sfmt("library %s;\nimport Greeting;\n\nfunction Add(a, b: Integer): Integer; { Result := a + b; }\n\n"
                        "function Greet(name: String): Integer;\n{\n  WriteLn(Hello(name));\n  Result := Length(name);\n}\n\n"
                        "exports Add, Greet;\n\n{\n}\n", id));
    else if (lib)
        write_file(sfmt("%s/src/lib.pas", name),
                   sfmt("library %s;\nimport Greeting;\n\nfunction Add(a, b: Integer): Integer;\nbegin\n  Result := a + b;\nend;\n\n"
                        "function Greet(name: String): Integer;\nbegin\n  WriteLn(Hello(name));\n  Result := Length(name);\nend;\n\n"
                        "exports Add, Greet;\n\nbegin\nend.\n", id));
    else if (pps)
        write_file(sfmt("%s/src/main.pps", name), sfmt("program %s;\nimport Greeting;\n{\n  WriteLn(Hello('%s'));\n}\n", id, base));
    else
        write_file(sfmt("%s/src/main.pas", name), sfmt("program %s;\nimport Greeting;\nbegin\n  WriteLn(Hello('%s'));\nend.\n", id, base));
    /* a test: a program that uses the module; its output is compared with greeting.expected */
    if (pps)
        write_file(sfmt("%s/tests/greeting.pps", name), "program GreetingTest;\nimport Greeting;\n{\n  WriteLn(Hello('test'));\n}\n");
    else
        write_file(sfmt("%s/tests/greeting.pas", name), "program GreetingTest;\nimport Greeting;\nbegin\n  WriteLn(Hello('test'));\nend.\n");
    write_file(sfmt("%s/tests/greeting.expected", name), "Hello, test!\n");
    printf("     Created %s '%s'\n", lib ? (stat ? "static library" : "library") : "program", name);
    return 0;
}

/* ---- the project ---- */
/* a path for messages: relative to the project */
static const char *rel(const char *p) {
    size_t n = strlen(proj.dir);
    return n && strncmp(p, proj.dir, n) == 0 ? p + n : p;
}

static void load_project_only(void) {
    char *ini = project_find(".");
    if (!ini) die("no ppc.ini in this directory or above (make a project with 'ppm new <name>')");
    char *err = NULL;
    if (project_load(ini, &proj, &err)) die("%s", err);
    if (project_module_dirs(&proj, &module_dirs, &err)) die("%s", err);
}

static void load(void) {
    load_project_only();
    char *err = NULL;
    if (proj.is_modules) die("'%s' has only modules (kind = modules): nothing to build or run ('ppm check' and 'ppm test' work)", proj.name);
    if (!proj.main) die("%s: no %s (src/%s.pas or src/%s.pps; or 'main = ...' in [project])", proj.ini_path,
                        proj.is_library ? "library" : "program", proj.is_library ? "lib" : "main", proj.is_library ? "lib" : "main");
    (void)err;
    char *word = NULL, *lname = NULL;
    source_header(proj.main, &word, &lname);
    if (proj.is_library && !(word && STREQ(word, "library") && lname))
        die("%s: kind = library, but '%s' does not start with 'library Name;'", proj.ini_path, proj.main);
    if (!proj.is_library && word && (STREQ(word, "library") || STREQ(word, "module")))
        die("%s: kind = program, but '%s' is a %s", proj.ini_path, proj.main, word);
}

static char *lib_name(void) {
    char *word, *name;
    source_header(proj.main, &word, &name);
    return name;
}

static char *target(void) { return sfmt("%starget/", proj.dir); }
static char *output(void) {
    if (proj.is_library) return sfmt("%slib%s.%s", target(), lib_name(), proj.is_static ? "a" : "so");
    return sfmt("%s%s", target(), proj.name);
}

static char *joined(StrVec *v, const char *sep) {
    Buf b = {0};
    for (int i = 0; i < v->len; i++) buf_printf(&b, "%s%s", i ? sep : "", v->items[i]);
    return xstrdup(buf_str(&b));
}

/* the options for ppc: [build] options, the module paths, [link] */
static void ppc_args(StrVec *av, int with_link) {
    vpush(*av, (char *)ppc_exe);
    StrVec opts = ini_split(ini_get(&proj.ini, "build", "options"), " \t");
    for (int i = 0; i < opts.len; i++) vpush(*av, opts.items[i]);
    if (module_dirs.len) {
        vpush(*av, "--module-paths");
        vpush(*av, joined(&module_dirs, ":"));
    }
    if (!with_link) return;
    StrVec so = ini_split(ini_get(&proj.ini, "link", "shared-objects"), ",;");
    StrVec sl = ini_split(ini_get(&proj.ini, "link", "static-libraries"), ",;");
    for (int i = 0; i < so.len; i++)
        if (so.items[i][0] != '/') so.items[i] = sfmt("%s%s", proj.dir, so.items[i]);
    for (int i = 0; i < sl.len; i++)
        if (sl.items[i][0] != '/') sl.items[i] = sfmt("%s%s", proj.dir, sl.items[i]);
    if (so.len) vpush(*av, sfmt("--shared-objects=%s", joined(&so, ";")));
    if (sl.len) vpush(*av, sfmt("--static-libraries=%s", joined(&sl, ";")));
}

static int build(void) {
    mkdirs(target());
    StrVec av = {0};
    int static_lib = proj.is_library && proj.is_static;
    ppc_args(&av, !static_lib); /* a static library is not linked */
    if (static_lib) vpush(av, "--static");
    char *out = output();
    vpush(av, "-o");
    vpush(av, out);
    vpush(av, proj.main);
    const char *ver = ini_get(&proj.ini, "project", "version");
    printf("   Compiling %s %s (%s)\n", proj.name, ver ? ver : "", proj.is_library ? (static_lib ? "static library" : "shared library") : "program");
    fflush(stdout);
    if (run(&av, NULL)) {
        fprintf(stderr, "ppm: could not build '%s'\n", proj.name);
        return 1;
    }
    printf("    Finished %s\n", rel(out));
    return 0;
}

static int cmd_build(void) {
    load();
    return build();
}

static int cmd_run(int argc, char **argv) {
    load();
    if (proj.is_library) die("'%s' is a library: it cannot be run ('ppm test' runs its tests)", proj.name);
    if (build()) return 1;
    StrVec av = {0};
    vpush(av, output());
    int i = 0;
    if (i < argc && STREQ(argv[i], "--")) i++;
    for (; i < argc; i++) vpush(av, argv[i]);
    printf("     Running %s\n", rel(output()));
    fflush(stdout);
    return run(&av, NULL);
}

/* the source files (.pas, .pps) of a directory, sorted */
static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static StrVec sources(const char *dir) {
    StrVec v = {0};
    DIR *d = opendir(dir);
    struct dirent *e;
    while (d && (e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n > 4 && (STREQ(e->d_name + n - 4, ".pas") || STREQ(e->d_name + n - 4, ".pps"))) vpush(v, sfmt("%s%s", dir, e->d_name));
    }
    if (d) closedir(d);
    if (v.len) qsort(v.items, (size_t)v.len, sizeof(char *), cmp_str);
    return v;
}

static int cmd_check(void) {
    load_project_only();
    StrVec files = sources(sfmt("%ssrc/", proj.dir));
    int bad = 0;
    for (int i = 0; i < files.len; i++) {
        StrVec av = {0};
        ppc_args(&av, 0);
        vpush(av, "--check");
        vpush(av, files.items[i]);
        printf("    Checking %s\n", rel(files.items[i]));
        fflush(stdout);
        if (run(&av, NULL)) bad++;
    }
    if (bad) {
        fprintf(stderr, "ppm: %d file(s) with errors\n", bad);
        return 1;
    }
    printf("    Finished no errors\n");
    return 0;
}

static char *read_text(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b = {0};
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof tmp, f)) > 0) buf_putn(&b, tmp, n);
    fclose(f);
    return xstrdup(buf_str(&b));
}

/* tests/<name>.pas|pps: programs that import the project's modules (and use a library through 'external');
   each runs in the project directory, its output is compared with tests/<name>.expected (if there) */
static int cmd_test(void) {
    if (project_find(".")) load_project_only();
    if (!proj.is_modules) {
        load();
        if (build()) return 1;
    }
    StrVec tests = sources(sfmt("%stests/", proj.dir));
    char *tdir = sfmt("%stests/", target());
    mkdirs(tdir);
    int passed = 0, failed = 0;
    for (int i = 0; i < tests.len; i++) {
        char *f = tests.items[i];
        char *base = xstrdup(strrchr(f, '/') + 1);
        base[strlen(base) - 4] = 0;
        char *word, *nm;
        source_header(f, &word, &nm);
        if (word && STREQ(word, "module")) continue; /* a helper module of the tests */
        char *exe = sfmt("%s%s", tdir, base), *outf = sfmt("%s%s.out", tdir, base);
        StrVec av = {0};
        ppc_args(&av, 1);
        if (proj.is_library) vpush(av, sfmt(proj.is_static ? "--static-libraries=%s" : "--shared-objects=%s", output()));
        vpush(av, "-o");
        vpush(av, exe);
        vpush(av, f);
        printf("test %s ... ", base);
        fflush(stdout);
        if (run(&av, NULL)) {
            printf("FAILED (does not compile)\n");
            failed++;
            continue;
        }
        StrVec rv = {0};
        vpush(rv, "sh");
        vpush(rv, "-c");
        vpush(rv, sfmt("'%s' > '%s' 2>&1 < /dev/null", exe, outf));
        int rc = run(&rv, proj.dir[0] ? proj.dir : NULL);
        char *exp = read_text(sfmt("%stests/%s.expected", proj.dir, base));
        char *got = read_text(outf);
        if (rc != 0) printf("FAILED (exit code %d)\n", rc), failed++;
        else if (exp && (!got || !STREQ(exp, got))) printf("FAILED (output differs from tests/%s.expected, see %s)\n", base, rel(outf)), failed++;
        else printf("ok\n"), passed++;
    }
    printf("\ntest result: %s. %d passed; %d failed\n", failed ? "FAILED" : "ok", passed, failed);
    return failed ? 1 : 0;
}

static int rm_entry(const char *p, const struct stat *s, int flag, struct FTW *f) {
    (void)s, (void)flag, (void)f;
    return remove(p);
}
static int cmd_clean(void) {
    char *ini = project_find(".");
    if (!ini) die("no ppc.ini in this directory or above");
    char *err;
    if (project_load(ini, &proj, &err)) die("%s", err);
    if (exists(target())) nftw(target(), rm_entry, 16, FTW_DEPTH | FTW_PHYS);
    printf("     Removed %s\n", rel(target()));
    return 0;
}

static void usage(void) {
    fprintf(stderr,
            "ppm - Perfect Pascal Manager (projects with a ppc.ini)\n"
            "usage: ppm <command>\n"
            "  new <name> [--lib] [--static] [--pps]   a new project (program, shared or static library; .pps files)\n"
            "  build                                   build into target/\n"
            "  run [-- args]                           build and run the program\n"
            "  check                                   check the program or library and every module in src/\n"
            "  test                                    build and run tests/*.pas|pps (output compared with .expected)\n"
            "  clean                                   remove target/\n");
    exit(2);
}

int main(int argc, char **argv) {
    if (argc < 2) usage();
    find_ppc(argv[0]);
    const char *c = argv[1];
    if (STREQ(c, "new")) return cmd_new(argc - 2, argv + 2);
    if (STREQ(c, "build")) return cmd_build();
    if (STREQ(c, "run")) return cmd_run(argc - 2, argv + 2);
    if (STREQ(c, "check")) return cmd_check();
    if (STREQ(c, "test")) return cmd_test();
    if (STREQ(c, "clean")) return cmd_clean();
    if (STREQ(c, "--help") || STREQ(c, "-h") || STREQ(c, "help")) usage();
    die("unknown command '%s' (new, build, run, check, test, clean)", c);
}
