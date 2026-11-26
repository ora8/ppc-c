#!/usr/bin/env python3
"""pas2pps: convert Perfect Pascal from the Pascal syntax (.pas) to the brace syntax (.pps).

    tools/pas2pps.py file.pas ...        writes file.pps next to each file
    tools/pas2pps.py -o out.pps file.pas
    tools/pas2pps.py - < file.pas        writes to standard output

The conversion works on tokens; comments, strings and the layout stay as they are:

* function -> func, procedure -> proc, begin -> {, end -> }
* record, class(...), = interface(...) and case ... of get an opening {
* the program's final 'end.' becomes '}', a module's final 'end.' is dropped
* the ';' after a routine header (or its last directive) is dropped when the body follows
"""
import re
import sys

KEYWORDS = set("""and array as begin case class const constructor destructor div do downto else end for function
goto if implementation in inherited interface is label mod nil none some optional not object of or packed
procedure program property record repeat set shl shr then to type unit until uses var while with xor try
loop val vao default result ok error module import""".split())
DIRECTIVES = {"virtual", "override", "abstract", "forward", "overload", "auto", "reintroduce", "static", "inline"}
ROUTINE = {"function", "procedure", "constructor", "destructor", "operator"}


class Tok:
    def __init__(self, kind, text, start, end):
        self.kind, self.text, self.start, self.end = kind, text, start, end  # kind: id kw num str sym


def tokens(src):
    """The tokens of src (comments and white space are skipped, their text stays in src)."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c.isspace():
            i += 1
        elif src.startswith("{*", i):
            j = src.find("*}", i + 2)
            i = n if j < 0 else j + 2
        elif src.startswith("(*", i):
            j = src.find("*)", i + 2)
            i = n if j < 0 else j + 2
        elif src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
        elif c.isalpha() or c == "_":
            m = re.compile(r"[A-Za-z_][A-Za-z0-9_]*").match(src, i)
            w = m.group(0)
            out.append(Tok("kw" if w in KEYWORDS else "id", w, i, m.end()))
            i = m.end()
        elif c.isdigit() or c == "$":
            m = re.compile(r"\$[0-9A-Fa-f]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?").match(src, i)
            out.append(Tok("num", m.group(0), i, m.end()))
            i = m.end()
        elif c in "'#":
            j = i
            while j < n and src[j] in "'#":
                if src[j] == "'":
                    j += 1
                    while j < n:
                        if src[j] == "'":
                            if j + 1 < n and src[j + 1] == "'":
                                j += 2
                                continue
                            j += 1
                            break
                        j += 1
                else:
                    m = re.compile(r"#(\$[0-9A-Fa-f]+|[0-9]+)").match(src, j)
                    j = m.end() if m else j + 1
            out.append(Tok("str", src[i:j], i, j))
            i = j
        else:
            for s in (":=", "<=", ">=", "<>", ".."):
                if src.startswith(s, i):
                    out.append(Tok("sym", s, i, i + 2))
                    i += 2
                    break
            else:
                if c in "{}":
                    raise SystemExit(f"'{c}' outside a comment at offset {i}: is this already the brace syntax?")
                out.append(Tok("sym", c, i, i + 1))
                i += 1
    return out


def convert(src):
    t = tokens(src)
    rep = {}  # token index -> replacement text ('' = drop)
    after = {}  # token index -> text inserted after the token

    def kw(k, w):
        return 0 <= k < len(t) and t[k].kind == "kw" and t[k].text == w

    def sym(k, s):
        return 0 <= k < len(t) and t[k].kind == "sym" and t[k].text == s

    def skip_parens(k):  # k at '(' -> index after the matching ')'
        depth = 0
        while k < len(t):
            if sym(k, "("):
                depth += 1
            elif sym(k, ")"):
                depth -= 1
                if depth == 0:
                    return k + 1
            k += 1
        return k

    def header_semicolon(k):
        # the header ends at the first ';' (or a lambda's 'begin') at paren depth 0; then directives
        j, d = k + 1, 0
        while j < len(t):
            if sym(j, "("):
                d += 1
            elif sym(j, ")"):
                d -= 1
            elif d == 0 and (sym(j, ";") or kw(j, "begin")):
                break
            j += 1
        if j >= len(t) or kw(j, "begin"):
            return  # a lambda: 'function(x: Integer): Integer begin ... end'
        last = j
        j += 1
        while j < len(t):
            if (t[j].kind == "id" and t[j].text in DIRECTIVES) or kw(j, "const"):
                if sym(j + 1, ";"):
                    last = j + 1
                    j += 2
                    continue
            break
        if kw(j, "begin"):
            rep[last] = ""

    is_module = len(t) > 0 and kw(0, "module")
    pending_case = []  # paren depth of open 'case' selectors
    opens = []  # what each open '{' belongs to: body (of a routine), block, type, case
    depth = 0
    for k, tok in enumerate(t):
        if sym(k, "("):
            depth += 1
        elif sym(k, ")"):
            depth -= 1
        if tok.kind == "id" and tok.text == "operator" and kw(k - 1, "class"):
            header_semicolon(k)  # class operator Add(a, b: TVec): TVec;
            continue
        if tok.kind != "kw":
            continue
        w = tok.text
        if w == "function":
            rep[k] = "func"
        elif w == "procedure":
            rep[k] = "proc"
        elif w == "begin":
            rep[k] = "{"
            opens.append("block" if any(o in ("block", "body", "case") for o in opens) else "body")
        elif w == "end":
            kind = opens.pop() if opens else None
            if kind in ("body", "type") and sym(k + 1, ";") and k + 2 < len(t):
                rep[k + 1] = ""  # 'end;' of a routine body or a type: '}' needs no ';'
            if sym(k + 1, ".") and k + 2 == len(t):  # the end of the file
                if is_module:
                    rep[k] = ""
                    rep[k + 1] = ""
                else:
                    rep[k] = "}"
                    rep[k + 1] = ""
            else:
                rep[k] = "}"
        elif w == "record":
            after[k] = " {"
            opens.append("type")
        elif w == "class":
            # a class type only after '=' (TA = class ...); otherwise 'class function', 'class var', 'T: class'
            if not (sym(k - 1, "=") or kw(k - 1, "packed")) or sym(k + 1, ";") or kw(k + 1, "of"):
                continue  # class;  class of T
            if sym(k + 1, "("):
                after[skip_parens(k + 1) - 1] = " {"
            else:
                after[k] = " {"
            opens.append("type")
        elif w == "interface" and sym(k - 1, "="):
            if sym(k + 1, "("):
                after[skip_parens(k + 1) - 1] = " {"
            else:
                after[k] = " {"
            opens.append("type")
        elif w == "case":
            pending_case.append(depth)
        elif w == "of" and pending_case and pending_case[-1] == depth and not kw(k - 1, "array") and not kw(k - 1, "set") \
                and not kw(k - 1, "class"):
            pending_case.pop()
            after[k] = " {"
            opens.append("case")
        if w in ROUTINE and not (k > 0 and (t[k - 1].text in (":", "=", "(", ",", ":=", "of", "to") and t[k - 1].kind in ("sym", "kw"))):
            header_semicolon(k)  # a routine declaration (not a procedural type 'x: function: Integer')
    out, pos = [], 0
    for k, tok in enumerate(t):
        out.append(src[pos:tok.start])
        r = rep.get(k)
        if r is None:
            out.append(tok.text)
        elif r == "" and tok.text == ";" and not src[tok.end:tok.end + 1].isspace() and src[tok.end:tok.end + 1] not in ("", "\n"):
            out.append(" ")  # 'x;begin' -> 'x {'
        else:
            out.append(r)
        if k in after:
            out.append(after[k])
        pos = tok.end
    out.append(src[pos:])
    text = "".join(out)
    if is_module:
        text = re.sub(r"\n[ \t]*\n?\Z", "\n", text.rstrip() + "\n")
    return re.sub(r"[ \t]+\n", "\n", text)


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    if argv == ["-"]:
        sys.stdout.write(convert(sys.stdin.read()))
        return 0
    out = None
    if argv[0] == "-o":
        out, argv = argv[1], argv[2:]
    for f in argv:
        dst = out or re.sub(r"\.pas$", "", f) + ".pps"
        with open(f, encoding="utf-8") as fh:
            src = fh.read()
        with open(dst, "w", encoding="utf-8") as fh:
            fh.write(convert(src))
        print(f"{f} -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
