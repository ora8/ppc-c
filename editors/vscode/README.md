# Perfect Pascal for VS Code

Support for [Perfect Pascal](../../README.md) (`ppc`) in Visual Studio Code:

* syntax highlighting (`val`/`var`/`vao`, `optional`, `result`, `reference to`, …), comment
  toggling (`//`, `{* *}`), brackets, `begin`/`end` folding and indentation;
* through the language server `ppc --lsp`: errors and warnings while you type (also borrow and
  nil errors), hover, go to definition (F12), find all references (Shift+F12), rename (F2) and
  completion (after `.` and for names);
* commands **Perfect Pascal: Run File** (also the ▷ button above the editor), **Check File** and
  **Restart Language Server**.

## Install

The `ppc` compiler must be installed (`make` in the repository root; put `ppc` on the PATH or set
`ppc.path`). Then build and install the extension:

```bash
cd editors/vscode
npm install
npm run package                                   # makes perfect-pascal-0.1.5.vsix
code --install-extension perfect-pascal-0.1.5.vsix
```

## Settings

| Setting | Meaning |
|---|---|
| `ppc.path` | the compiler: a command on the PATH (default `ppc`) or the full path |
| `ppc.modulePaths` | directories for `import` (like `ppc --module-paths`); relative ones belong to the workspace folder |
| `ppc.sharedObjects` | `.so` files that *Run File* links (like `ppc --shared-objects`) |
| `ppc.staticLibraries` | `.a` files that *Run File* links (like `ppc --static-libraries`) |
| `ppc.runArguments` | extra options for *Run File*, e.g. `-O3` or `-g` |
| `ppc.trace.server` | `messages` / `verbose`: log the LSP traffic (Output: Perfect Pascal) |

Files ending in `.pas` are Perfect Pascal files, files ending in `.pps` use the brace syntax
(`{ }` blocks, `if` without `then`; comments `{* *}`, `(* *)` and `//`) with their own highlighting. If another Pascal extension claims them, choose
*Perfect Pascal* with *Change Language Mode*.
