/* main.c - ppc compiler driver */
#include "ast.h"
#include <limits.h>
#include <sys/wait.h>
#include <unistd.h>

extern const char *PPC_STD;
static const char *PRELUDE =
    "type\n"
    "  TObject = class\n"
    "    constructor Create;\n"
    "    destructor Destroy; virtual;\n"
    "    procedure Free;\n"
    "    function ClassName: String; const;\n"
    "  end;\n"
    "constructor TObject.Create; begin end;\n"
    "destructor TObject.Destroy; begin end;\n"
    "procedure TObject.Free; begin if Assigned(Self) then Destroy end;\n"
    "function TObject.ClassName: String; const; begin Result := __ClassName(Self) end;\n";

static VEC(Block *) mods;     /* loaded modules, dependencies first */
static VEC(char *) mod_names; /* parallel to mods */
static VEC(char *) loading;   /* modules being loaded (cycle detection) */
static const char *mod_dir;
static char *read_file(const char *path);
static VEC(char *) module_paths;  /* --module-paths: more directories for 'import' (each ends with '/') */
static const char *src_override_path, *src_override; /* --check --stdin of a module file: its text */
static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) fclose(f);
    return f != NULL;
}
/* the directory of a path with its '/' ("" for a file in the current directory) */
static char *dir_of(const char *path) {
    const char *s = path ? strrchr(path, '/') : NULL;
    return s ? xstrndup(path, (size_t)(s - path + 1)) : xstrdup("");
}
static VEC(char *) shared_objects; /* --shared-objects: .so files the program is linked with */
static void add_shared_objects(const char *list) { /* 'libMathLib.so;lib/libz.so' (';' or ':' between them) */
    const char *p = list;
    while (*p) {
        size_t n = strcspn(p, ";:");
        if (n) vpush(shared_objects, xstrndup(p, n));
        p += n + (p[n] ? 1 : 0);
    }
}
static void add_module_paths(const char *list) { /* 'lib:../shared/modules' (':' or ';' between them) */
    const char *p = list;
    while (*p) {
        const char *e = strpbrk(p, ":;");
        size_t n = e ? (size_t)(e - p) : strlen(p);
        if (n) {
            char *d = xstrndup(p, n);
            vpush(module_paths, d[n - 1] == '/' ? d : sfmt("%s/", d));
        }
        p += n + (e ? 1 : 0);
    }
}

static void load_module(Ident *name) {
    for (int i = 0; i < mod_names.len; i++)
        if (STREQ(mod_names.items[i], name->name)) return; /* loaded once */
    for (int i = 0; i < loading.len; i++)
        if (STREQ(loading.items[i], name->name)) error_at(name->pos, "modules import each other in a cycle: '%s'", name->name);
    const char *src;
    char *file;
    if (STREQ(name->name, "ppc_std")) {
        src = PPC_STD;
        file = "ppc_std.pas";
    } else {
        /* next to the importing file, then in the --module-paths; in each directory first the file in the
           syntax of the importing file (M.pps for a .pps file), then the other one */
        int br = is_brace_file(name->pos.file);
        VEC(char *) dirs = {0}, tried = {0};
        vpush(dirs, name->pos.file && strchr(name->pos.file, '/') ? dir_of(name->pos.file) : sfmt("%s/", mod_dir));
        for (int i = 0; i < module_paths.len; i++) vpush(dirs, module_paths.items[i]);
        file = NULL;
        for (int i = 0; i < dirs.len && !file; i++)
            for (int k = 0; k < 2 && !file; k++) {
                char *f = sfmt("%s%s.%s", STREQ(dirs.items[i], "./") ? "" : dirs.items[i], name->name, (k == 0) == br ? "pps" : "pas");
                if (STREQ(dirs.items[i], "./")) f = sfmt("./%s", f);
                if (file_exists(f)) file = f;
                else vpush(tried, f);
            }
        if (!file) {
            Buf b = {0};
            for (int i = 0; i < tried.len; i++) buf_printf(&b, "%s%s", i == 0 ? "" : i == tried.len - 1 ? " and " : ", ", tried.items[i]);
            error_at(name->pos, "module '%s' not found (looked for %s)%s", name->name, buf_str(&b),
                     module_paths.len ? "" : "; other directories are given with --module-paths dir1:dir2");
        }
        src = src_override_path && STREQ(file, src_override_path) ? src_override : read_file(file);
    }
    vpush(loading, name->name);
    const char *save = g_filename;
    g_filename = file;
    Ident mname;
    g_allow_external = src == PPC_STD;
    Block *b = parse_module(src, &mname);
    g_allow_external = 0;
    if (!STREQ(mname.name, name->name))
        error_at(mname.pos, "the file declares 'module %s', but it is imported as '%s'", mname.name, name->name);
    g_filename = save;
    for (int i = 0; i < b->uses.len; i++) load_module(&b->uses.items[i]);
    loading.len--;
    vpush(mods, b);
    vpush(mod_names, name->name);
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ppc: cannot open '%s'\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = xmalloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "ppc: cannot read '%s'\n", path);
        exit(2);
    }
    buf[n] = 0;
    fclose(f);
    return buf;
}

static void usage(void) {
    fprintf(stderr,
            "ppc - Perfect Pascal Compiler (case-sensitive, C backend, borrow checker)\n"
            "usage: ppc [options] file.pas\n"
            "  -o <file>          output executable (default: file name without .pas); for a library the .so\n"
            "                     (default: lib<Name>.so next to the file, with the C header <Name>.h)\n"
            "  -S                 only emit C code (<out>.c), do not compile\n"
            "  -k                 keep the generated C file next to the executable\n"
            "  --no-borrow-check  disable the borrow checker\n"
            "  --no-nil-check     replace the compile-time nil analysis by runtime nil checks\n"
            "  --no-range-check   no runtime check of array and string indexes\n"
            "  --cc <compiler>    C compiler to use (default: $CC or gcc)\n"
            "  --module-paths <dir1:dir2>  more directories for 'import' (after the directory of the importing file)\n"
            "  --shared-objects <a.so;b.so>  link the shared objects (their C functions are declared 'external')\n"
            "  -O0 .. -O3         optimization level passed to the C compiler (default -O2)\n"
            "  -g                 debug information for gdb (with -O0 and the C file kept): break and step in the .pas file\n"
            "  --no-line          no #line directives (C compiler messages and gdb then refer to the generated C file)\n"
            "  --check            only check the program (syntax, types, borrow checker), produce nothing\n"
            "  --stdin            read the source from standard input (file.pas still names it and its directory)\n"
            "  --lsp              run as a language server (LSP over standard input/output) for editors\n");
    exit(2);
}

static char *read_stdin(void) {
    Buf b = {0};
    char tmp[65536];
    size_t n;
    while ((n = fread(tmp, 1, sizeof tmp, stdin)) > 0) buf_putn(&b, tmp, n);
    return xstrdup(buf_str(&b));
}

int lsp_main(const char *self, const char *module_paths);

static void print_hover(void) { /* --hover: the description on standard output (also after errors) */
    const char *h = hover_text();
    if (h) fprintf(stdout, "%s\n", h);
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc >= 2 && STREQ(argv[1], "--lsp")) { /* ppc --lsp [--module-paths dir1:dir2] */
        const char *mp = NULL;
        for (int i = 2; i + 1 < argc; i++)
            if (STREQ(argv[i], "--module-paths") || STREQ(argv[i], "--module_paths")) mp = argv[i + 1];
        for (int i = 2; i < argc; i++)
            if (strncmp(argv[i], "--module-paths=", 15) == 0) mp = argv[i] + 15;
        return lsp_main(argv[0], mp);
    }
    int check_only = 0, from_stdin = 0;
    const char *input = NULL, *output = NULL, *cc = getenv("CC");
    const char *opt = "-O2";
    int debug = 0;
    int emit_only = 0, keep = 0, borrow = 1;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (STREQ(a, "-o") && i + 1 < argc) output = argv[++i];
        else if (STREQ(a, "-S")) emit_only = 1;
        else if (STREQ(a, "-k")) keep = 1;
        else if (STREQ(a, "-g")) debug = 1, keep = 1;
        else if (STREQ(a, "--no-line")) g_line_dirs = 0;
        else if (STREQ(a, "--check")) check_only = 1;
        else if (STREQ(a, "--stdin")) from_stdin = 1;
        else if (STREQ(a, "--complete")) g_complete = 1, check_only = 1; /* the marker __ppc_complete__ is in the text */
        else if ((STREQ(a, "--hover") || STREQ(a, "--definition")) && i + 1 < argc) {
            g_def_mode = STREQ(a, "--definition");
            if (sscanf(argv[++i], "%d:%d", &g_hover_line, &g_hover_col) != 2) usage();
            check_only = 1;
            atexit(print_hover);
        }
        else if (STREQ(a, "--no-borrow-check")) borrow = 0;
        else if (STREQ(a, "--no-nil-check")) g_nil_check = 0;
        else if (STREQ(a, "--no-range-check")) g_range_check = 0;
        else if (STREQ(a, "--cc") && i + 1 < argc) cc = argv[++i];
        else if ((STREQ(a, "--module-paths") || STREQ(a, "--module_paths")) && i + 1 < argc) add_module_paths(argv[++i]);
        else if (strncmp(a, "--module-paths=", 15) == 0) add_module_paths(a + 15);
        else if ((STREQ(a, "--shared-objects") || STREQ(a, "--shared_objects")) && i + 1 < argc) add_shared_objects(argv[++i]);
        else if (strncmp(a, "--shared-objects=", 17) == 0) add_shared_objects(a + 17);
        else if (a[0] == '-' && a[1] == 'O') opt = a;
        else if (a[0] == '-') usage();
        else if (!input) input = a;
        else usage();
    }
    if (!input) usage();
    if (!cc || !*cc) cc = "gcc";

    char *base;
    {
        const char *dot = strrchr(input, '.');
        const char *slash = strrchr(input, '/');
        if (dot && (!slash || dot > slash)) base = xstrndup(input, (size_t)(dot - input));
        else base = xstrdup(input);
    }
    const char *out_given = output;
    if (!output) output = base;

    g_filename = "<prelude>";
    Block *prelude = parse_decls_only(PRELUDE);
    g_filename = input;
    g_hover_file = input;
    char *src = from_stdin ? read_stdin() : read_file(input);
    char *pname;
    Pos end_pos = {0};
    {
        const char *slash = strrchr(input, '/');
        mod_dir = slash ? xstrndup(input, (size_t)(slash - input)) : ".";
    }
    char *mname = check_only ? module_header_name(src) : NULL;
    Block *prog_block;
    if (mname) { /* --check of a module (an editor): checked as if a program imported it */
        prog_block = NEW(Block);
        pname = xstrdup("main");
        src_override_path = input;
        src_override = src;
        Ident mi = {mname, {1, 1, input}};
        vpush(prog_block->uses, mi);
    } else
        prog_block = parse_program(src, &pname, &end_pos);
    if (g_errors) {
        fprintf(stderr, "%s: %d syntax error(s), compilation aborted\n", input, g_errors);
        return 1;
    }
    /* modules: 'import Name' loads Name.pas next to the importing file or in the --module-paths
       (ppc_std is built in), dependencies first */
    for (int i = 0; i < prog_block->uses.len; i++) load_module(&prog_block->uses.items[i]);
    g_filename = input;
    if (g_errors) {
        fprintf(stderr, "%s: %d error(s) in modules, compilation aborted\n", input, g_errors);
        return 1;
    }
    Program *prog = sema_program(prelude, mods.items, mods.len, prog_block, pname);
    if (g_errors) {
        fprintf(stderr, "%s: %d error(s), compilation aborted\n", input, g_errors);
        return 1;
    }
    prog->end_pos = end_pos;
    if (g_hover_line) return 0; /* --hover: the description is printed at exit */

    g_borrow_check = borrow;
    if (borrow || g_nil_check) {
        int errs = borrow_check(prog);
        if (errs) {
            fprintf(stderr, "%s: %d error(s) found by the borrow/nil checker, compilation aborted\n", input, errs);
            return 1;
        }
    }

    if (check_only) return 0;
    VEC(char *) rpaths = {0}; /* the directories of the shared objects: found there at run time */
    for (int i = 0; i < shared_objects.len; i++) {
        char rp[PATH_MAX];
        if (!realpath(shared_objects.items[i], rp)) {
            fprintf(stderr, "ppc: shared object '%s' not found (--shared-objects)\n", shared_objects.items[i]);
            return 1;
        }
        char *d = xstrdup(rp);
        *strrchr(d, '/') = 0;
        vpush(rpaths, sfmt("-Wl,-rpath,%s", *d ? d : "/"));
    }
    char *hfile = NULL;
    if (prog->is_library) { /* library Name;  ->  libName.so and Name.h (next to the file, or next to -o) */
        const char *slash = strrchr(input, '/');
        char *dir = slash ? xstrndup(input, (size_t)(slash - input + 1)) : xstrdup("");
        if (!out_given) output = sfmt("%slib%s.so", dir, pname);
        const char *os = strrchr(output, '/');
        char *odir = os ? xstrndup(output, (size_t)(os - output + 1)) : xstrdup("");
        hfile = sfmt("%s%s.h", odir, pname);
        FILE *h = fopen(hfile, "w");
        if (!h) {
            fprintf(stderr, "ppc: cannot write '%s'\n", hfile);
            return 2;
        }
        codegen_header(prog, h);
        fclose(h);
    }
    char *cfile = (emit_only || keep) ? sfmt("%s.c", output) : sfmt("/tmp/ppc_%d_%s.c", (int)getpid(), "out");
    FILE *f = fopen(cfile, "w");
    if (!f) {
        fprintf(stderr, "ppc: cannot write '%s'\n", cfile);
        return 2;
    }
    g_cfile = cfile;
    codegen(prog, f);
    fclose(f);
    if (emit_only) return 0;

    pid_t pid = fork();
    if (pid == 0) {
        const char **av = (const char **)xmalloc(sizeof(char *) * (size_t)(24 + 2 * shared_objects.len));
        int n = 0;
        av[n++] = cc;
        av[n++] = "-std=gnu11";
        if (debug) av[n++] = "-O0", av[n++] = "-g";
        else av[n++] = opt;
        av[n++] = "-w";
        av[n++] = "-fwrapv";
        av[n++] = "-fno-strict-aliasing";
        if (prog->is_library) av[n++] = "-shared", av[n++] = "-fPIC", av[n++] = "-fvisibility=hidden"; /* only the exports are visible */
        av[n++] = "-o";
        av[n++] = output;
        av[n++] = cfile;
        for (int i = 0; i < shared_objects.len; i++) av[n++] = shared_objects.items[i], av[n++] = rpaths.items[i];
        av[n++] = "-lm";
        av[n] = NULL;
        execvp(cc, (char *const *)av);
        fprintf(stderr, "ppc: cannot run C compiler '%s'\n", cc);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    if (!keep) remove(cfile);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        fprintf(stderr, "ppc: C compilation failed\n");
        return 1;
    }
    return 0;
}
