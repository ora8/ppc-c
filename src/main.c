/* main.c - ppp compiler driver */
#include "ast.h"
#include <sys/wait.h>
#include <unistd.h>

static const char *PRELUDE =
    "type\n"
    "  TObject = class\n"
    "    constructor Create;\n"
    "    destructor Destroy; virtual;\n"
    "    procedure Free;\n"
    "    function ClassName: String;\n"
    "  end;\n"
    "constructor TObject.Create; begin end;\n"
    "destructor TObject.Destroy; begin end;\n"
    "procedure TObject.Free; begin if Self <> nil then Destroy end;\n"
    "function TObject.ClassName: String; begin Result := __ClassName(Self) end;\n";

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ppp: cannot open '%s'\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = xmalloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "ppp: cannot read '%s'\n", path);
        exit(2);
    }
    buf[n] = 0;
    fclose(f);
    return buf;
}

static void usage(void) {
    fprintf(stderr,
            "ppp - Object Pascal compiler (case-sensitive, C backend, borrow checker)\n"
            "usage: ppp [options] file.pas\n"
            "  -o <file>          output executable (default: file name without .pas)\n"
            "  -S                 only emit C code (<out>.c), do not compile\n"
            "  -k                 keep the generated C file next to the executable\n"
            "  --no-borrow-check  disable the borrow checker\n"
            "  --cc <compiler>    C compiler to use (default: $CC or gcc)\n"
            "  -O0 .. -O3         optimization level passed to the C compiler (default -O2)\n");
    exit(2);
}

int main(int argc, char **argv) {
    const char *input = NULL, *output = NULL, *cc = getenv("CC");
    const char *opt = "-O2";
    int emit_only = 0, keep = 0, borrow = 1;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (STREQ(a, "-o") && i + 1 < argc) output = argv[++i];
        else if (STREQ(a, "-S")) emit_only = 1;
        else if (STREQ(a, "-k")) keep = 1;
        else if (STREQ(a, "--no-borrow-check")) borrow = 0;
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
    char *src = read_file(input);
    char *pname;
    Pos end_pos;
    Block *prog_block = parse_program(src, &pname, &end_pos);
    Program *prog = sema_program(prelude, prog_block, pname);
    prog->end_pos = end_pos;

    if (borrow) {
        int errs = borrow_check(prog);
        if (errs) {
            fprintf(stderr, "%s: %d borrow check error(s), compilation aborted\n", input, errs);
            return 1;
        }
    }

    char *cfile = (emit_only || keep) ? sfmt("%s.c", output) : sfmt("/tmp/ppp_%d_%s.c", (int)getpid(), "out");
    FILE *f = fopen(cfile, "w");
    if (!f) {
        fprintf(stderr, "ppp: cannot write '%s'\n", cfile);
        return 2;
    }
    codegen(prog, f);
    fclose(f);
    if (emit_only) return 0;

    pid_t pid = fork();
    if (pid == 0) {
        execlp(cc, cc, "-std=gnu11", opt, "-w", "-fwrapv", "-fno-strict-aliasing", "-o", output, cfile, "-lm",
               (char *)NULL);
        fprintf(stderr, "ppp: cannot run C compiler '%s'\n", cc);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    if (!keep) remove(cfile);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        fprintf(stderr, "ppp: C compilation failed\n");
        return 1;
    }
    return 0;
}
