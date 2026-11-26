/* main.c - ppc compiler driver */
#include "ast.h"
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
        file = sfmt("%s/%s.pas", mod_dir, name->name);
        FILE *f = fopen(file, "rb");
        if (!f) error_at(name->pos, "module '%s' not found (looked for %s)", name->name, file);
        fclose(f);
        src = read_file(file);
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
            "  -o <file>          output executable (default: file name without .pas)\n"
            "  -S                 only emit C code (<out>.c), do not compile\n"
            "  -k                 keep the generated C file next to the executable\n"
            "  --no-borrow-check  disable the borrow checker\n"
            "  --no-nil-check     replace the compile-time nil analysis by runtime nil checks\n"
            "  --no-range-check   no runtime check of array and string indexes\n"
            "  --cc <compiler>    C compiler to use (default: $CC or gcc)\n"
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

int lsp_main(const char *self);

static void print_hover(void) { /* --hover: the description on standard output (also after errors) */
    const char *h = hover_text();
    if (h) fprintf(stdout, "%s\n", h);
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc >= 2 && STREQ(argv[1], "--lsp")) return lsp_main(argv[0]);
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
    if (!output) output = base;

    g_filename = "<prelude>";
    Block *prelude = parse_decls_only(PRELUDE);
    g_filename = input;
    g_hover_file = input;
    char *src = from_stdin ? read_stdin() : read_file(input);
    char *pname;
    Pos end_pos;
    Block *prog_block = parse_program(src, &pname, &end_pos);
    if (g_errors) {
        fprintf(stderr, "%s: %d syntax error(s), compilation aborted\n", input, g_errors);
        return 1;
    }
    /* modules: 'import Name' loads Name.pas next to the program (ppc_std is built in), dependencies first */
    {
        const char *slash = strrchr(input, '/');
        mod_dir = slash ? xstrndup(input, (size_t)(slash - input)) : ".";
    }
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
        if (debug)
            execlp(cc, cc, "-std=gnu11", "-O0", "-g", "-w", "-fwrapv", "-fno-strict-aliasing", "-o", output, cfile, "-lm", (char *)NULL);
        else
            execlp(cc, cc, "-std=gnu11", opt, "-w", "-fwrapv", "-fno-strict-aliasing", "-o", output, cfile, "-lm", (char *)NULL);
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
