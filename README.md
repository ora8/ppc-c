# ppc – Perfect Pascal Compiler

`ppc` (**Perfect Pascal Compiler**) is the compiler for **Perfect Pascal**, an evolution of
Pascal and Delphi. The language takes the clear syntax of **Pascal** (`begin … end`, `:=`,
`procedure`/`function`, records, enumerations, subranges, `case`, `for … to/downto`) and the
object orientation of **Delphi** (classes with `constructor`/`destructor`, `virtual`/`override`,
`inherited`, properties, `is`/`as`, class methods, records with methods, generics with `<T>`).
Beyond that it develops both further – with ideas from Rust: ownership and borrowing instead of
manual memory management, `optional T` instead of `nil`, `result T` instead of exceptions,
stack and heap objects of the same class, and case-sensitive names.

Perfect Pascal is therefore **not** source compatible with Delphi or Free Pascal: what is
familiar looks familiar, but `var`/`val`/`vao`, mandatory initialization and the ownership rules
differ on purpose.

`ppc` translates Perfect Pascal to **C** and then calls `gcc` to produce a native program. The
compiler itself is written in C (C11, no dependencies).

Highlights:

* **Case-sensitive** (unlike standard Pascal).
* **Three kinds of variables:** `val` for values and stack objects, `var` for references without
  ownership and `vao` for owners of heap objects. A `vao` frees its object automatically when it
  leaves its scope (like `Box<T>` in Rust); a stack object is destroyed at the end of its block.
  Every variable is initialized in its declaration.
* **Borrow checker** modelled on Rust: checks ownership, borrows and the borrowing rules (many
  read-only *or* one changeable borrow) at compile time.
* **No pointers and no nil:** there is no `^`, `@`, `New`, `Dispose` and no `nil`. Values that
  may be missing have the type **`optional T`** (like `Option<T>` in Rust or `std::optional<T>`
  in C++) and must be unpacked before use.
* **No exceptions:** errors are return values of type **`result T`** (like `Result<T, E>` in
  Rust), passed on with `try`.
* **`T const`** makes variables, parameters and object references unchangeable; `; const;`
  marks methods that do not change `Self`.
* **Generics** (`TList<T>`, `Max<T: Comparable>`), **modules** (`module` / `import`) and a
  **standard library** (`import ppc_std`).
* **Class methods** (`class function`), **records with methods**, record and array literals,
  dynamic arrays (`array of T`).

## Building and using

```bash
make            # builds ./ppc
make test       # test suite (output, borrow/optional/stack/const/result/clone/generic errors, compile errors)
```

```bash
./ppc program.pas             # produces ./program
./ppc -o hello program.pas    # name of the program
./ppc -S program.pas          # only produce the C code (program.c)
./ppc -k program.pas          # keep the C file as well
./ppc --no-borrow-check x.pas # switch off the borrow checker
./ppc --no-nil-check x.pas    # no allocation analysis, check at run time instead
./ppc --no-range-check x.pas  # no run-time check of array/string indexes
./ppc --cc clang -O3 x.pas    # other C compiler / optimization
./ppc -g program.pas          # with debug information for gdb (-O0, keeps program.c)
./ppc --no-line x.pas         # no #line directives in the C code
./ppc --check x.pas           # only check (errors, warnings), produce nothing
./ppc --lsp                   # language server for editors (see below)
```

### Editor support: `ppc --lsp`

`ppc --lsp` is a language server (LSP over standard input/output). Editors show the compiler's
messages while you type – syntax and type errors, borrow and nil errors, warnings – on the
right line and column, also for unsaved changes. Every check runs `ppc --check` on the current
text; modules next to the file are found as usual, and an error inside an imported module is
shown at the top of the importing file.

Neovim (`init.lua`):

```lua
vim.api.nvim_create_autocmd('FileType', {
  pattern = 'pascal',
  callback = function()
    vim.lsp.start({ name = 'ppc', cmd = { 'ppc', '--lsp' }, root_dir = vim.fn.getcwd() })
  end,
})
```

Helix (`languages.toml`):

```toml
[language-server.ppc]
command = "ppc"
args = ["--lsp"]

[[language]]
name = "pascal"
language-servers = ["ppc"]
```

Emacs (eglot): `(add-to-list 'eglot-server-programs '(pascal-mode "ppc" "--lsp"))`.

Other editors: configure `ppc --lsp` as the language server for `.pas` files. Without an editor:
`ppc --check file.pas` checks a file (`--stdin` reads the text from standard input).

**Hover:** pointing at a name shows what it is – `(local) val Sum: Integer`, `(parameter) var
Extra: Integer`, `(field of TBox) Size: Integer`, `function TShape.Area(Scale: Real): Real;
const; virtual`, `type TColor = (Red, Green, Blue)`, `const Limit = 42: Integer`, built-ins –
for uses and declarations. On the command line: `ppc --hover LINE:COL file.pas`. Completion and
go-to-definition are not there yet.

### Debugging

The generated C code carries `#line` directives: every C line of a statement names the line of
the `.pas` file it comes from. So messages of the C compiler, AddressSanitizer reports and the
debugger refer to the Pascal source. With `-g` the program is built for gdb:

```bash
./ppc -g program.pas
gdb ./program
(gdb) break program.pas:38      # stop at line 38 of the Pascal source
(gdb) run
(gdb) next                      # step by Pascal lines
(gdb) info locals               # local variables (C names: L_Sum, __f.L_Sum in routines with a frame)
```

The C names follow the Pascal names: locals `L_Name` (or `L12_Name` in nested blocks), parameters
`p_Name`, globals `G_Name`, routines `R_Name`, methods `M<n>_Name`, `self` for `Self`. The code
that frees owners at the end of a routine maps to the C file.

## Error messages

The compiler does not stop at the first error but reports as many as possible at once:

```
prog.pas(4,15) Error: ')' expected but number 1 found
prog.pas(6,8) Error: expression expected but ':=' found
prog.pas(15,6) Error: expression expected but 'then' found
prog.pas: 3 syntax error(s), compilation aborted
```

* **Syntax errors:** after an error the parser skips the broken statement up to the next `;` or
  the end of the block (a declaration up to its end, a routine together with its body) and reads
  on.
* **Type errors:** if the text is syntactically fine, the compiler checks every statement and
  every declaration on its own; a broken one is skipped. If the declaration of a variable is
  broken, its later uses are not reported again.
* **Borrow checker:** also reports all errors it finds (it only runs when there are no syntax or
  type errors).
* After 20 errors the compiler stops (`too many errors (20), stopping`).

## Case sensitivity

* Keywords are written in **lower case**: `program begin end if then class val var vao …`
  (`Begin` is an identifier, not a keyword).
* Identifiers are case-sensitive: `Count` and `count` are two different names.
* Predefined names in Delphi spelling: `Integer`, `Boolean`, `Char`, `Real`, `String`,
  `Byte`, `Word`, `Int64`, `Cardinal`, `True`, `False`, `MaxInt`, `Pi`,
  `WriteLn`, `Write`, `ReadLn`, `Read`, `Length`, `Ord`, `Chr`, `Inc`, `Dec`,
  `TObject`, `Create`, `Destroy`, `Free`, `ClassName`, `Self`, `Result`, `Exit`, `Break`, `Continue` etc.
* Directives/modifiers in lower case: `virtual`, `override`, `abstract`, `forward`, `private`,
  `public`, `property … read … write …`, `optional`, `some`, `none`, `default`, `result`, `ok`,
  `error`, `try`, `auto`.

## Classes

Reference types, implicitly derived from `TObject`:
`constructor Create`, `destructor Destroy; override;`, `Free`, `ClassName`,
`virtual`/`override`/`abstract`, `inherited`, properties with field or method access,
`is`/`as`, `Self`, visibility sections (see below), forward classes
(`TFoo = class;`). Objects are only created in an initializer: as a **stack object**
`val k := TCircle.Create(1, 2, 3.0)` or as a **heap object** `vao k := TCircle.Create(1, 2, 3.0)`.

### Visibility: `private`, `protected`, `public`

```pascal
type
  TAccount = class
  private
    FBalance: Integer;                 { only the methods of TAccount }
    procedure Log(s: String);
  protected
    FOwner: String;                    { also the methods of descendants }
  public
    constructor Create(AOwner: String);
    property Balance: Integer read FBalance;
  end;
```

* `private`: only the class's own methods (and routines nested in them) can use the member –
  like `strict private` in Delphi; `strict` is accepted and changes nothing. In Delphi, plain
  `private` is open to the whole unit; Perfect Pascal enforces it per class.
* `protected`: the class and its descendants. `public` and `published`: everyone (the default).
* It applies to fields, methods, constructors and properties, also through implicit `Self` and in record
  literals: `the field "FBalance" of "TAccount" is private (only its own methods can use it)`.
* Records know `private` and `public`. A property may read or write a private field.

### Interfaces

An interface lists methods that unrelated classes can implement (as in Delphi, without
reference counting: interface values follow the normal `val`/`var`/`vao` rules):

```pascal
type
  IShape = interface
    function Area: Real; const;
    function Name: String; const;
  end;
  ISolid = interface(IShape)                 { extends IShape }
    function Volume(h: Real): Real; const;
  end;
  TCircle = class(IShape)                    { parent TObject, implements IShape }
    ...
  end;
  TCube = class(TShapeBase, ISolid, IScalable) { one parent class, then interfaces }
    ...
  end;

procedure Show(s: IShape const);
begin
  WriteLn(s.Name, ' ', s.Area:0:1, ' (', s.ClassName, ')');
end;

vao c := TCircle.Create(1);
Show(c);                                     { a TCircle is an IShape }
var s: IShape := c;                          { reference to the object }
vao shapes: array of optional IShape := ...; { owns objects of different classes }
if s is ISolid then ...                      { run-time test; also 'as' }
```

* The class (or an ancestor) needs a method with the same name, parameters and result for every
  interface method, and `const` where the interface says `const`; a virtual method is called
  in the version of the object's class.
* An interface value is a normal object reference: owned by `vao`, borrowed by `var` and
  parameters, `optional IShape` for an empty one; `ClassName`, `is TCircle`, `as TCircle` work.
* Interfaces can be generic (`ILess<T> = interface function Less(a, b: T): Boolean; const;
  end`) and serve as constraints: `function Total<T: IShape>(...)`.
* A call through an interface looks up the class's table for that interface (a short search in
  the VMT); calls on the class type itself stay direct.

### Class references: `class of T` and virtual constructors

```pascal
type
  TShape = class
    constructor Create(ASize: Integer); virtual;     { virtual constructor }
    class function Kind: String;
  end;
  TShapeClass = class of TShape;
  TSquare = class(TShape)
    constructor Create(ASize: Integer); override;
  end;

function Make(c: TShapeClass; n: Integer): TShape;
begin
  Result := c.Create(n);          { an object of the class in c, with its constructor }
end;

val kinds: array of TShapeClass := (TSquare, TCircle);
vao s := Make(TSquare, 2);
WriteLn(s.ClassType = TSquare, ' ', s is TSquare, ' ', TSquare.ClassName);
```

* A class name used as a value is its class reference; a `class of TShape` holds `TShape` or a
  descendant. Through it: constructors (`c.Create`, the class's own version if the constructor is
  `virtual`), class methods (of the declared class) and `ClassName`; `obj is c`, `obj as c`,
  `c = TSquare` and `obj.ClassType` (the class of an object) work too.
* An object made through a class reference must be owned by `vao` (its size is only known at
  run time, so it cannot be a stack object). An unset class reference (`default`, e.g. in an
  array) stops with run-time error 210 when it is used.

### Class fields: `class var`

A class field exists once for the class, not once per object (like `class var` in Delphi):

```pascal
type
  TCounter = class
  private
    class var FCreated: Integer;
  public
    class var Prefix: String;
    class var Default: optional TItem;
  var                                   { back to ordinary fields }
    Id: Integer;
    constructor Create;
    class function Created: Integer;
  end;

constructor TCounter.Create;
begin
  Inc(FCreated);                        { inside methods and class methods without a prefix }
  Id := FCreated;
end;

TCounter.Prefix := '#';                 { from outside through the type (or an object) }
```

* As in Delphi, `class var` starts a section: the following fields are class fields too, until a
  visibility keyword, a method, a property or `var`.
* Class fields are global variables: zero at the start, objects in them must be `optional` and
  are freed when the program ends, `var` references are not allowed. Visibility applies;
  descendants share the field of their ancestor, every instance of a generic class has its own.
  Records can have class fields too.

### Indexers: `property Item[i: Integer]: T … default;`

A property with index parameters (an *array property* in Delphi) is read and written with
brackets; with `default` the property name can be left out:

```pascal
type
  TGrid = class
    function GetCell(x, y: Integer): Integer; const;
    procedure SetCell(x, y: Integer; v: Integer);
    property Cell[x, y: Integer]: Integer read GetCell write SetCell; default;
  end;

g.Cell[2, 1] := 7;              { SetCell(2, 1, 7) }
g[0, 0] := g[2, 1] + 1;         { default indexer }
if l[0] is some x then …;       { TList<T> in ppc_std: property Item[i]: optional T; default }
m['Anna'] := 30;                { TMap<String, Integer> }
```

* The reader is a function with the index parameters and the property's type as result, the
  writer a procedure with the index parameters and then the value (for an `optional T`
  property the writer may take a `T`). They are ordinary methods: ownership, borrowing and
  visibility apply as for direct calls.
* Inside the class an indexer is used without `Self.` (`Cell[x, y]`). `x.Cell` without an index
  is an error, and only indexed properties can be `default`.

### Class methods: `class function` / `class procedure`

As in Delphi, class methods belong to the class, not to an object. They have no `Self` and are
called through the type name (also for records):

```pascal
type
  TAccount = class
    Balance: Integer;
    class function Open(AName: String const): TAccount;
  end;
  TPoint = record
    X, Y: Integer;
    class function Origin: TPoint;
  end;

class function TAccount.Open(AName: String const): TAccount;
begin Result := TAccount.Create(AName, 10); end;
class function TPoint.Origin: TPoint; begin Result := (X: 0; Y: 0); end;
…
vao k := TAccount.Open('Anna');
val p := TPoint.Origin;
```

* In the body, other class methods can be called without a prefix; fields, normal methods,
  `Self` and `inherited` do not exist there.
* Class methods cannot be `virtual`, `abstract`, `override` or `; const;`; constructors and
  destructors are not class methods.
* The implementation repeats `class` (`class function TAccount.Open …`).

## Variables: `val`, `var` and `vao`

Besides `const` and `type` there are three variable sections. **Every variable is initialized in
its declaration with `:=`**; the type may be left out, then it comes from the initializer.

| | what the variable holds | owns? | frees? | Rust | C++ |
|---|---|---|---|---|---|
| **`val`** | the **value itself** (number, string, record, array, stack object) | yes | at the end of the block (stack object: `Destroy`) | `let x: T` | `T x` |
| **`vao`** | an **owning reference** to an object or a value on the heap | yes | at the end of the scope | `Box<T>` | `std::unique_ptr<T>` |
| **`var`** | an **ordinary reference** to something that belongs to someone else | no | never | `&mut T` (`T const`: `&T`) | `T&` (`T const`: `const T&`) |

Parameters follow the same scheme: `vao x: TFoo` takes over the ownership, `var x: T` is a
changeable reference, `x: T` borrows an object or copies a simple value.

| Declaration | Meaning |
|---|---|
| `val i := 5;` / `val i: Integer := 5;` | **value** (numbers, strings, records, arrays, `optional` values) |
| `val v: TVector := default;` | zero value of the type (0, `''`, `False`, `none`, all fields 0) |
| `val s := TCircle.Create(1, 2, 3.0);` | **stack object**: lives in the stack frame, `Destroy` at the end of the block, see below |
| `vao k := TCircle.Create(1, 2, 3.0);` | **owner**: `k` owns the object and frees it at the end |
| `vao n: optional TCircle := none;` | owner that may own nothing (yet) |
| `vao I: Integer := 42;` | **heap value**: a value on the heap, see below |
| `var r: TCircle := k;` | **reference without ownership** (borrow) to an object; frees nothing |
| `var x: Integer := i;` | **reference to a variable**: `x` is a second name for `i` (like `int&` in C++) |

```pascal
procedure Demo;
vao
  k := TCircle.Create(4, 5, 1.0);     { owns the object }
var
  r: TFigure := k;                    { borrows it }
val
  n := 3;
begin
  Show(r);                            { same type, virtual methods, polymorphism }
end;                                  { k is freed here, r is not }
```

* A class variable in `val` is a stack object and must be created with a constructor;
  `optional` only works with `vao` or `var`.
* Initializers may be any expressions. They are evaluated when the declaration is executed: on
  entry to the routine or the block (see "Blocks").
* **Owners (`vao`)** free their object **automatically** when they leave their scope (routine,
  block, program) – unless the ownership was passed on before: as a function result
  (`Result := h`), through a `var` parameter, to a `vao` parameter, into a field or another
  owner, with `Take`. When an owner is overwritten, its old object is freed; when an object is
  freed, its owning fields are freed with it; temporary objects that nobody takes are freed at
  the end of the statement.
* `h.Free` is allowed to free **earlier**. Using it afterwards and a double `Free` are errors.
* **References (`var`)** own nothing. For objects they are borrows; for values they refer to
  another variable (or a field, an array element, a heap value) – reading and writing through
  `x` acts directly on the original. The initializer must be a variable, not a computed value.
  The borrowing rules apply to both (see "Borrow checker").
* `var` for parameters (`procedure P(var x: Integer)`) means passing by reference, as always.

### Stack objects: `val k := TClass.Create(...)`

When speed matters, an object can live directly in the stack frame (globals: in the data
segment) – without `malloc` and `free`. Class, methods, inheritance and VMT are the same as for
heap objects:

```pascal
procedure Draw(F: TFigure);                 { takes stack and heap objects }

procedure Local;
val k := TCircle.Create(1, 2.5);            { constructed in the stack frame }
begin
  Draw(k);                                  { borrow, virtual methods as usual }
  k.Name := TFigure.Create(99);             { owning fields work too }
  var f: TFigure := k;                      { reference to the stack object }
  begin f.X := 5; end;
end;                                        { k.Destroy runs here, k.Name is freed }
```

* The object is constructed in its declaration and destroyed automatically at the end of its
  block (`Destroy`, then its owning fields are freed) – also on `Break`, `Exit` and `try`. In a
  loop body a fresh object is created in every iteration.
* **Moving** into another stack object works like in Rust: `val b := k` copies the object byte
  for byte into `b` (no constructor, no `malloc`); `k` is dead afterwards – using it and borrows
  of `k` are errors (`use of moved stack object 'k'`), and only `b` is destroyed at the end of
  the block. If the move happens on one path only, the program remembers whether `k` still has
  to be destroyed.
* **Moving to the heap** (like `Box::new` in Rust): when an owner takes the stack object –
  `vao h := k`, `Result := k`, `obj.Field := k`, a `vao` parameter – the program allocates heap
  memory once and copies the object into it byte for byte (owning fields move along, nothing is
  cloned). `k` is dead afterwards, as with every move. So an object can be built quickly on the
  stack and only be given to the heap when it has to outlive the block:

  ```pascal
  function Build(n: Integer): TK;
  val k := TK.Create(n);
  begin
    k.Part := TK.Create(n + 100);
    Result := k;                    { the object with its part moves to the heap }
  end;
  ```
* **From the heap to the stack:** `val k := h` (or a function result, `val k := Make(1)`) copies
  the object into the stack frame and gives the heap memory back; `h` is empty afterwards. The
  class must match exactly – if the object is a subclass at run time, the program stops with
  `Runtime error 219: a TBar object does not fit into stack object 's' (TFoo)`.
* In short:

  | from → to | `val` (stack) | `vao` (heap) | `var` (reference) |
  |---|---|---|---|
  | `val k` | move (`memcpy`) | move to the heap | borrow |
  | `vao h` | move to the stack | move (pointer) | borrow |
  | `var r` | only as a copy: `val k := r.Clone` | only as a copy: `vao h := r.Clone` | borrow |

  You cannot move out of a reference, because the object still belongs to its owner.
* A stack object cannot be reassigned or freed. A copy that keeps the original:
  `vao t := k.Clone`.
* Borrows (`var r: TFigure := k`, parameters) are allowed, but must not outlive the block: the
  borrow checker reports storing them in global variables, fields or function results.
* The declared type and the class of the constructor must be the same
  (`val f: TFigure := TCircle.Create` is an error, because the object is created in its exact
  size).
* Measurement: 50 million short-lived objects take 0.08 s with `-O2` as stack objects and
  0.32 s as heap objects.

### Dynamic arrays: `array of T`

A dynamic array owns a buffer on the heap that grows and shrinks with `SetLength` (index from 0,
as in Delphi):

```pascal
val numbers: array of Integer := (1, 2, 3);
vao customers: array of optional TCustomer := default;   { empty }
begin
  SetLength(numbers, 5);                    { new elements are 0 }
  numbers[4] := 42;
  WriteLn(Length(numbers), ' ', High(numbers));   { 5 4 }
  SetLength(customers, 1);
  customers[0] := TCustomer.Create('Anna');
end;                                        { buffer and customers are freed }

function Sum(a: array of Integer): Integer;       { borrow, no copy }
procedure Fill(var a: array of Integer; n: Integer);   { may change the length }
```

* `Length(a)`, `High(a)` (= `Length(a) - 1`), `Low(a)` (= 0); every access is checked
  (`Runtime error 201: … index 3 is outside 0..2`).
* `SetLength` frees elements that are cut off (if they own objects); new ones start with the
  zero value or `none`. Because the buffer may move, borrows of elements are invalid afterwards –
  the borrow checker reports their use
  (`… used after its owner 'a' was resized by SetLength at line 3`).
* The array owns its buffer: `b := a` **moves** (`a` is empty afterwards), `a.Clone` copies (and
  clones the objects it contains); as a parameter it is borrowed.
* Works in generic code too, e.g. a `TStack<T>` with `Items: array of optional T`.

### Arrays and records that own objects

An array or record with owning elements or fields (`array[1..9] of optional TK`,
`record L: optional TK; end`) owns these objects – also in a `val` variable:

```pascal
val
  a: array[1..2] of optional TK := default;
  q: TPair := default;
  z: TPair := default;
begin
  a[1] := TK.Create(1);        { the object now belongs to the array }
  q := Make(4);
  z := q;                      { move like in Rust: q is empty afterwards (all fields none) }
  q := Make(5);                { the old q is freed }
end;                           { a, q and z free their objects here }
```

* Such values are **never copied** but moved: assignment and initialization empty the source.
  You cannot move out of constants, `is some` bindings and parameters, and they must not be
  copied into a literal.
* As parameters they are only borrowed read-only (by reference, like `T const`); `var`
  parameters change the original; a **`vao` parameter takes them over** (`procedure
  Keep(vao o: TOrder)`): the argument is moved in (the caller's variable is empty afterwards) and
  the routine frees it at its end unless it moves it on.
* A **`var` result** of such a type refers into a container instead of copying
  (`function Get(i: Integer): var optional TOrder` with `Result := Items[i]`; `Result := none`
  for no element). Look at it with `if l.Get(0) is some o then …` – `o` is a read-only view that
  the borrow checker ties to `l` (`l.Clear` while `o` is used is an error); taking it over
  (`val o := l.Get(0)`) is an error – take it out with `Extract`/`Take` instead.
* From a fresh value (`if l.Extract(0) is some o`), `o` takes the value over and frees it when
  the `if` ends.
* Objects that have fields of such types free them with themselves; an unused function result
  (`Make(9);`) is freed at once.
* Such a function result must be stored before it is used: `Make(1).Info` or `Show(Make(2))` are
  errors (otherwise it would never be freed) – write `val r := Make(1)`.

### Copying: `Clone`

Numbers, strings, enumerations and records/arrays **without** objects are copied by `b := a`.
Everything that owns objects is **moved**. A copy of it only exists explicitly with `Clone`
(like `.clone()` in Rust) – written by hand or generated by the compiler with `; auto;`
(like `#[derive(Clone)]`):

```pascal
type
  TPerson = class
    Name: String;
    Address: optional TAddress;              { owned: is cloned along }
    Boss: var optional TPerson;              { reference: keeps pointing to the same boss }
    function Clone: TPerson; const; virtual; auto;
  end;
  TEmployee = class(TPerson)                 { inherits Clone; its fields are copied too }
    Office: optional TAddress;
  end;
  TPair = record
    A, B: optional TAddress;
    function Clone: TPair; const; auto;
  end;
…
vao dup := anna.Clone;                       { a new object, dup owns it }
var p: TPerson := employee;
vao k := p.Clone;                            { a TEmployee with all its fields }
val q := pair.Clone;
```

* The generated `Clone` copies all values, clones every owning field (also in arrays and
  records) with its own `Clone` and lets `var` fields point to the same object.
* For classes it copies the **whole object of its actual class** – also descendants that only
  inherit `Clone`. If an owning field (also in a descendant) has a type without `Clone`, the
  compiler reports it.
* No constructor runs; the result is new ownership that the caller takes over.
* Form: `function Clone: T; const; auto;` (for classes also with `virtual`; the result may be an
  ancestor class). A hand-written `Clone` is an ordinary const method.
* **Arrays** have `Clone` built in: `val d := b.Clone;` copies the array and clones every object
  in it (the element class needs a `Clone`); without objects it is a simple copy. Like every
  value that owns objects, the result must be stored.

### Heap values: values in `vao`

Numbers, strings, records and arrays can also belong to an owner on the heap (like `Box<T>` in
Rust). They are used like a normal value; at the end of the scope they are freed:

```pascal
vao
  I: Integer := 20;            { creates the heap cell }
  V: TVector := default;       { record on the heap }
begin
  I := I + 1;
  Duplicate(I);                { var parameter: reference to the heap value }
  V.Scale(2);
end;                           { I and V are freed here }
```

* Every assignment stores a new **value** in the cell; `J := I` between two heap values copies
  the value. After `I.Free` the next assignment creates the cell again.
* Useful mainly for large records or arrays that should not live in the stack frame.

### Strings

`String` has no length limit. Strings are values, as in Pascal: every variable owns its own
buffer, an assignment copies, and the buffer is freed at the end of the scope – no manual memory
management and no reference counting:

```pascal
val
  a := 'Hello';
  b := a;                   { a copy }
begin
  b[1] := 'J';              { changes only b: Hello Jello }
  for val i := 1 to 200 do b := b + 'abc';
  WriteLn(Length(b));       { 605 }
end;
```

* `s[i]` is indexed from 1 to `Length(s)` and checked at run time; `SetLength`, `Delete`,
  `Insert`, `Copy`, `Pos`, `UpperCase`, `IntToStr` … work on strings of any length.
* Value parameters get their own copy; `s: String const` passes by reference without a copy.
* Strings in records, arrays, optional values, results and objects are copied, freed and cloned
  with them.
* Intermediate results (`a + b`, `IntToStr(i)`, function results) are temporaries that are freed
  after the statement that uses them.
* Changing the length (`SetLength`, `Delete`, `Insert`) invalidates borrows of characters.

**Format** (as in Delphi) builds a string from a format and the values in brackets:

```pascal
WriteLn(Format('%d items for %s', [n, Name]));             { 3 items for Anna }
WriteLn(Format('[%5d] [%-6s] [%8.2f] [%x] [%n]', [42, 'ab', 3.14159, 255, 1234567.891]));
{ [   42] [ab    ] [    3.14] [FF] [1,234,567.89] }
WriteLn(Format('%1:s %0:s', ['world', 'hello']));          { hello world }
```

* Specifiers `%[index:][-][width][.precision]type`: `d` (whole number), `u`, `x` (hex), `f`
  (fixed, 2 decimals by default), `e`, `g`, `n` (with thousands separators), `s` (strings,
  characters, Boolean, enumeration names; `.precision` cuts), `%%` for `%`.
* With a literal format the compiler checks the specifiers against the values:
  `Format: '%d' needs a whole number, but argument 1 is "String"`, too few or too many values.
  A format computed at run time that does not fit stops with run-time error 220.

### Sets: `set of T`

```pascal
type
  TColor = (Red, Green, Blue, Yellow);
  TColors = set of TColor;
val
  warm: TColors := [Red, Yellow];
  all: TColors := [Red..Yellow];
  vowels: set of Char := ['a', 'e', 'i', 'o', 'u'];
begin
  if Green in all - warm then …;      { difference; also + (union), * (intersection) }
  if warm <= all then …;              { subset; >= superset, = and <> }
  Include(warm, Green);
  Exclude(warm, Red);
  for val c in warm do WriteLn(c);    { the members in order }
end;
```

* The element type is an enumeration, `Char`, `Boolean`, `Byte` or a subrange (`set of 0..9`)
  with at most 256 values. A set is a value of 256 bits: copied by assignment, no allocation.
* `[...]` takes its type from where it is used (`[]` is the empty set); `val s := [Green, Blue]`
  is a set of the elements' type. Constant elements outside the range are errors, others stop
  with run-time error 201 when they are added.

### Comparing values: `=` and `<>`

Records, arrays (fixed and dynamic), optional values and enumerations with data are compared
**element by element** with `=` and `<>`: strings by their content, objects by identity (the same
object), optional values are equal if both are empty or both hold equal values.

```pascal
val a: TPoint := (X: 1; Y: 2);
if a = (X: 1; Y: 2) then …;          { record literal on one side }
if anna.Tags <> bert.Tags then …;    { array of String }
if s = Circle(2.0) then …;           { enumeration with data }
```

Such types also satisfy the constraint `Equatable`. Records with a function value or a `result`
inside cannot be compared (`"TR" cannot be compared with '='`).

### Literals and `default`

```pascal
val
  p: TPoint := (X: 1; Y: 2);                     { record literal }
  a: array[1..3] of Integer := (10, 20, 30);    { array literal }
  z: TBig := default;                            { all 0 / '' / none }
```

Literals adapt to the expected type (initializer, assignment, argument, `Result`).

## Type from the initializer, and blocks

Without a type, the initializer determines the type; after that it is fixed:

```pascal
val
  N := 3.5;              { Real }
  S := 'Hello';          { String }
vao
  I := 5;                { Integer on the heap }
begin
  N := 'x';              { error: N is Real }
end;
```

* No type can be inferred from `none` and `default` (write `val x: optional T := none`).
* `for val i := 1 to n do …` declares the loop variable directly in the loop.
* `while val i := 0; i < n do …` declares a variable for the whole `while` loop: it is created
  **once** before the first check, keeps its value across iterations and is no longer visible
  after the loop (also with `var` and `vao`; a `vao` is freed after the loop):

  ```pascal
  while val i := 0; i < 3 do
  begin
    Write(i, ' ');
    Inc(i);
  end;
  ```

**Blocks with their own variables:** `val`, `var`, `vao` and `const` sections may stand before a
nested `begin … end`. They form a scope of their own – also as a loop body, then with fresh
variables in every iteration:

```pascal
begin
  val j := 10;
  vao s := TFoo.Create(1);   { belongs to this block only }
  begin
    WriteLn(j, ' ', s.V);
  end;                       { s is freed here, j is no longer visible }

  while i < 3 do
    val q := i * 100;
    begin
      WriteLn(q);
      Inc(i);
    end;
end;
```

* Owners of the block free at the end of the block; borrows of their objects are invalid
  afterwards (the borrow checker reports their use).
* Routines, types and typed constants are not allowed in blocks.

## Unchangeable: `const` and `T const`

**`const`** only declares constants known at compile time (also usable for array bounds) and
typed constants:

```pascal
const
  Max = 100;
  Name = 'ppc';
  Limit: Integer = 5;          { read-only }
```

For unchangeable values that are only known at run time there is the type modifier
**`T const`** – after the type, like `; const;` for methods:

```pascal
procedure P(n: Integer);
val
  Square: Integer const := n * n;        { unchangeable, value at run time }
  w: TVector const := v;                 { unchangeable copy of a record }
var
  reader: TPerson const := anna;         { read-only borrow of an object }
begin
  Square := 1;                           { error }
  w.X := 1;                              { error: part of a constant }
  w.Scale(2);                            { error: Scale is no const method }
  WriteLn(w.Len);                        { allowed: const method }
```

* Everything inside the value is unchangeable: record fields and array elements. On constant
  records only **const methods** may be called (see below).
* `var x: Integer const := y` is a read-only reference to `y`.
* Fields cannot be `const`.

**`T const` for parameters:** the parameter is passed **by reference** (no copy) and is
read-only – like `const T&` in C++ or `&T` in Rust. Useful for strings, records and arrays;
expressions as arguments are put into a temporary copy:

```pascal
function Sum(A: TBig const): Integer;               { no copy of the array }
procedure Show(S: String const; V: TVector const);
…
WriteLn(Twice(Square + 1));
```

| Parameter | Passing | Changing allowed |
|---|---|---|
| `x: T` | copy (objects: borrow) | yes, only the copy (objects: yes) |
| `x: T const` | reference | no |
| `var x: T` / `out x: T` | reference | yes, at the caller |
| `vao x: TFoo` | the object is handed over | yes, the parameter owns it |

The borrow checker treats `T const` parameters as read-only borrows: `Q(x, x)` with a `var` and
a `T const` parameter is an error.

### Constant methods and routines: `; const;`

`const;` as a directive at the end of the declaration, like `virtual;` (corresponds to
`void f() const` in C++). The implementation repeats it: `function TVector.Len: Real; const;`.

```pascal
type
  TVector = record
    X, Y: Real;
    procedure Scale(F: Real);              { may change Self }
    function Len: Real; const;             { does not change Self }
  end;
  TAccount = class
    function Description: String; const; virtual;
  end;

function SquareOf(N: Integer): Integer; const;   { free routine without side effects }
begin Result := N * N; end;
```

* **In a const method** `Self` is read-only: no assignment to fields (also for classes), and
  only other const methods may be called on `Self`.
* **On constant values** – `T const` variables and parameters, `Self` in const methods – only
  const methods can be called.
* **A free routine with `; const;`** has no side effects on the program state: it changes no
  global or outer variables, only calls other const routines and only calls const methods on
  non-local objects. It may use parameters, local variables and newly created objects freely;
  output with `WriteLn` is allowed.
* Declaration and implementation must both be `const`; an `override` must keep the const
  property of the overridden method. `ClassName` is a const method.

### Read-only object references: `TFoo const`

`TFoo const` is a reference through which an object can **only be read** – like `&T` compared
with `&mut T` in Rust:

```pascal
procedure Show(P: TPerson const);
function Older(A, B: TPerson const): var TPerson const;
function BestFriend: var optional TPerson const; const;
var
  reader: TPerson const := anna;                { changeable -> read-only: allowed }
begin
  WriteLn(reader.Info);                         { const method: allowed }
  reader.Birthday;                              { error: no const method }
  reader.Age := 1;                              { error: the object is read-only }
```

* **Everything reached through it is read-only:** fields of class type
  (`P.Friend is some f` → `f` is `TPerson const`), array elements, results of `as`.
* A read-only reference can **not** be turned into a changeable one – neither by assignment nor
  as an argument, `var` parameter or function result.
* `Self` is read-only in const methods; therefore a const method can only hand out
  `var … const` references to its fields.

## Generic types: `TList<T>`

Records and classes can have type parameters (design in [docs/generics.md](docs/generics.md)).
For every type combination used, the compiler creates its own instance – checked at compile time
and without any cost at run time:

```pascal
type
  TPair<A, B> = record
    First: A;
    Second: B;
  end;
  TList<T> = class
    Items: array[1..10] of optional T;
    Size: Integer;
    procedure Add(vao x: T);
    function Get(i: Integer): var optional T;
  end;
  TNode<T> = class
    Value: T;
    Next: optional TNode<T>;                 { recursive }
  end;
  TCustomerList = class(TList<TCustomer>)    { inherits from an instance }
    function Find(N: String const): var optional TCustomer;
  end;

procedure TList<T>.Add(vao x: T);           { methods repeat <T> }
begin
  Inc(Size);
  Items[Size] := x;                          { move, even if T is an object }
end;
…
val p: TPair<String, Integer> := (First: 'Anna'; Second: 30);
vao numbers := TList<Integer>.Create;
vao customers := TCustomerList.Create;
customers.Add(TCustomer.Create('Anna'));
if customers.Find('Anna') is some k then WriteLn(k.Name);    { k is a TCustomer }
```

* Generic types are declared at program (or module) level; records, classes, functions and
  procedures can be generic.

**Generic functions and procedures:**

```pascal
function Max<T: Comparable>(a, b: T): T;
begin
  if a > b then Result := a else Result := b;
end;

procedure Swap<T>(var a, b: T);
val h: T := a;
begin a := b; b := h; end;

function MakePair<A, B>(x: A; y: B): TPair<A, B>;
…
WriteLn(Max(3, 7), Max('Anna', 'Bert'));     { T from the arguments: Integer, String }
WriteLn(Max(3, 7.5));                        { Integer and Real: T = Real }
WriteLn(Max<Real>(1, 2));                    { T written out }
Swap(i, j);
val p := MakePair('x', 42);                  { TPair<Char, Integer> }
```

* The type arguments are inferred from the arguments – also through `optional T`, arrays,
  `result T` and generic types (`First<A, B>(p: TPair<A, B>)`). If that is not possible (e.g.
  when `T` only occurs in the result), they are written out: `Fresh<Integer>`.
* If the arguments contradict each other, the compiler reports it:
  `cannot infer 'T' of "Max": it is "Integer" from one argument and "Char" from another`.
* Recursion works.

**Constraints** decide which types may be used (as in Delphi: constraints of one parameter with
`,`, parameter groups with `;`):

```pascal
function Max<T: Comparable>(a, b: T): T;
function Equal<T: Equatable>(a, b: T): Boolean;
function CloneOf<T: Clone>(x: T): T;          { x.Clone }
type
  TPair<K: Comparable; V> = record … end;     { K constrained, V free }
  TZoo<T: TAnimal> = class … end;             { T must be TAnimal or a descendant }
```

| Constraint | allowed types |
|---|---|
| `class` | classes |
| `TBase` (a class name) | `TBase` and its descendants |
| `Copy` | values without objects (numbers, strings, enumerations, simple records/arrays) |
| `Clone` | classes and records with `Clone`, arrays of them |
| `Equatable` (`=`, `<>`) | numbers, Char, Boolean, strings, enumerations, classes (same object) |
| `Comparable` (`<`, `>` …) | numbers, Char, Boolean, strings, enumerations |
| `Numeric` (`+`, `-`, `*`, number literals, comparisons) | integer types, Real |
| `Hashable` (`=`, `<>`, `Hash(x)`) | numbers, Char, Boolean, enumerations, strings, sets, records and fixed arrays of them |

If a type does not fit, the compiler reports it at the call or the instance:
`"TK" does not satisfy 'T: Comparable' (required by Max<T>)`.

**The generic body is checked on its own** (as in Rust), even if the generic is never used: with
a `T` you may only do what its constraints allow. Without a constraint you can store, move, pass
it and put it into `optional T`, arrays or `result T` – but not compute with it, compare it or
call fields and methods:

```pascal
function Sum<T>(a, b: T): T;
begin Result := a + b; end;           { error: operator needs the constraint 'T: Numeric' (in Sum<T>) }

function Name<T>(x: T): String;
begin Result := x.Name; end;          { error: 'Name' is not known for type parameter "T" … }

function Factorial<T: Numeric>(n: T): T;
begin
  if n <= 1 then Result := 1 else Result := n * Factorial(n - 1);   { ok }
end;
```

* An error in the body is reported once this way – not again for every instance. With a base
  class as constraint (`T: TAnimal`) all fields and methods of `TAnimal` are available, with
  `T: class` those of `TObject`.
* **The borrow checker checks the body on its own too.** A `T` without `Copy` (or
  `Comparable`/`Numeric`, which only fit values) may be an object, so it is treated like one:
  assigning moves it, a moved `T` cannot be used again, and an element cannot be copied out of
  an array – exactly the rules for objects:

  ```pascal
  procedure Twice<T>(vao x: T);
  begin
    Keep(x);
    Keep(x);      { use of moved value 'x' (in the generic Twice: a type parameter without
  end;              'Copy' may be an object) }

  function First<T>(a: array of optional T): optional T;
  begin Result := a[0]; end;             { cannot move out of 'a[...]' ... use Take(a[...]) }

  function Twice<T: Copy>(x: T): TPair<T>;
  begin Result.A := x; Result.B := x; end;   { ok: T is a value }
  ```

  Such an error is reported once for the generic, not for every instance. Every instance is
  still checked with its actual types as well.
* For a plain value `T` (numbers, strings, records without objects), `vao x: T` is an ordinary
  pass and a `var` result is a copy. For an object `T` or a value that owns objects (a record
  with an object field, `array of String`, …) the ownership rules apply: `vao x: T` moves the
  argument in, a `var` result refers into the container. So `TList<TOrder>`, `TStack<TOrder>`
  or `TMap<String, TOrder>` work with `TOrder = record Name: String; Item: optional TItem; end`,
  and free the objects of their elements.
* `ClassName` returns the name of the instance, e.g. `'TList<TCustomer>'`.

### Generic methods

Classes and records that are not generic themselves can have generic methods (as in Delphi):

```pascal
type
  TPrinter = class
    Prefix: String;
    function Show<T>(x: T): String;
    function Pick<T: Comparable>(a, b: T): T; const;
    procedure Each<T>(a: array of T; Visit: reference to procedure(x: T));
    class function Make<T: class>(n: Integer): TPrinter;
  end;

function TPrinter.Pick<T>(a, b: T): T; const;   { the implementation repeats <T> }
begin
  if a < b then Result := b else Result := a;
end;

WriteLn(p.Pick(3, 9), p.Pick('b', 'a'));        { T from the arguments }
WriteLn(p.Show<Real>(2.5));                     { or written out }
vao q := TPrinter.Make<TItem>(7);               { class method }
```

* An instance is made for every type combination used; the body is checked once on its own
  (constraints, borrow checker), like a generic routine. Inside the class it is called without
  `Self.` (`Wrap<T>(x)`), descendants inherit it, visibility applies.
* Generic methods cannot be virtual, abstract, constructors or interface methods, and generic
  types cannot have generic methods yet.

## Modules: `module` and `import`

A program includes modules with **`import`**. A module is a file of its own that starts with
**`module Name;`** and has two sections, as Pascal units do:

* **`interface`** – the public declarations: constants, types (classes and records with their
  method headers), variables and the **headers** of routines;
* **`implementation`** – the definitions: the bodies of the routines and methods, plus private
  declarations (helper routines, types, variables) that importers do not see.

The module ends with `end.`:

```pascal
module Geometry;                        { file Geometry.pas }
import ppc_std;                         { imports come right after the header }

interface

type
  TPoint = record
    X, Y: Real;
    function Distance(b: TPoint): Real; const;
  end;

function TotalArea(l: TList<TFigure>): Real;   { header only }

implementation

function Square(x: Real): Real;         { private: not visible to importers }
begin Result := x * x; end;

function TPoint.Distance(b: TPoint): Real; const;
begin Result := Sqrt(Square(X - b.X) + Square(Y - b.Y)); end;

function TotalArea(l: TList<TFigure>): Real;
begin … end;

end.
```

```pascal
program Main;                           { file Main.pas in the same directory }
import ppc_std, Geometry;

val a: TPoint := (X: 0; Y: 0);
val b: TPoint := (X: 3; Y: 4);
begin
  WriteLn(a.Distance(b):0:1);           { 5.0 }
end.
```

* `import Name` looks for `Name.pas` in the directory of the program; the name after `module`
  must match the file name. Every module is loaded only once, its own imports first; modules
  that import each other (cycles) are an error.
* Everything in the interface is visible in the importing program; the implementation is
  private (`identifier not found "Square"`). Names of the program hide those of a module.
* Every routine declared in the interface must be defined in the implementation
  (`'P' is declared in the interface but not implemented`); method bodies and routine bodies in
  the interface are errors. Generic routines and generic types work the same way – their bodies in
  the implementation may use the private helpers.
* Errors in a module are reported with its file name: `Geometry.pas(12,5) Error: …`.
* A module has no main block (`begin … end`); it ends with `end.`. The old `uses` gives
  `'uses' was replaced by 'import'`.

### Standard library: `import ppc_std`

The standard library is itself a Perfect Pascal module ([lib/ppc_std.pas](lib/ppc_std.pas)), with
an interface and an implementation section, and is embedded into the compiler. It contains generic
containers and algorithms, string and math functions, whole-file I/O, time and program parameters:

```pascal
program Example;
import ppc_std;
val numbers: array of Integer := (5, 3, 9, 1, 7);
vao customers := TList<TCustomer>.Create;
vao ages := TMap<String, Integer>.Create;
begin
  Sort(numbers);                                     { 1 3 5 7 9 }
  if IndexOf(numbers, 7) is some p then WriteLn(p);  { 3 }
  customers.Add(TCustomer.Create('Anna'));
  if customers.Get(0) is some k then WriteLn(k.Name);
  ages.Put('Anna', 30);
  if ages.Get('Anna') is some a then WriteLn(a);
end.                                                 { all containers free their objects }
```

| Name | Contents |
|---|---|
| `TList<T>` | `Add(vao x)`, `Count`, `Get(i)` (borrowed), `Put(i, vao x)` (replace), `Extract(i)` (take out), `Clear`; `l[i]`, `l[i] := x`; index from 0 |
| `TStack<T>` | `Push(vao x)`, `Pop` (take out), `Peek` (look), `Count`, `IsEmpty` |
| `TMap<K: Comparable; V>` | `Put(k, vao v)` (insert/replace), `Get(k)`, `Contains(k)`, `Remove(k)`, `Count`; `m[k]`, `m[k] := v`; sorted by key, binary search |
| `THashMap<K: Hashable; V>` | the same with hashing: `Put`, `Get`, `Contains`, `Remove`, `Count`, `KeyList`, `Clear`, `m[k]`; O(1) per operation (100 000 keys: 7 ms instead of 3.4 s with `TMap`), no order |
| `Sort<T: Comparable>(var a: array of T)` | sorts ascending (quicksort) |
| `SortBy<T>(var a: array of T; Less: function(x, y: T): Boolean)` | sorts with a comparison function (stable) |
| `Filter<T: Copy>(a: array of T; Keep: function(x: T): Boolean): array of T` | the elements for which `Keep` is true |
| `IndexOf<T: Equatable>(a: array of T; x: T): optional Integer` | first position of `x`, or `none` |

**Strings**

| Name | Contents |
|---|---|
| `Trim`, `TrimLeft`, `TrimRight` | without spaces and control characters at the ends |
| `StartsWith(s, p)`, `EndsWith(s, p)`, `Contains(s, sub)` | Boolean tests |
| `PosFrom(sub, s, from)` | first position at or after `from` (0: none) |
| `ReplaceStr(s, old, new)` | replaces every occurrence |
| `Split(s, sep): array of String`, `Join(parts, sep)`, `SplitLines(s)` | splitting and joining |
| `ReverseString`, `DupeString(s, n)`, `PadLeft(s, w, c)`, `PadRight(s, w, c)`, `IntToHex(v, digits)` | building strings |
| `TryStrToInt(s): optional Int64`, `TryStrToFloat(s): optional Real`, `StrToFloat(s)` | parsing (`StrToFloat`: run-time error 106 if invalid) |
| `IsDigit`, `IsLetter`, `IsSpace` | character classes |

**Math:** `Pi`, `Min<T: Comparable>`, `Max`, `EnsureRange(x, lo, hi)`, `Floor`, `Ceil` (Int64),
`Power`, `Tan`, `ArcSin`, `ArcCos`, `ArcTan2`, `Log10`, `Log2`, `Hypot`, `Sign`, `IsNan`
(besides the built-in `Abs`, `Sqr`, `Sqrt`, `Sin`, `Cos`, `ArcTan`, `Exp`, `Ln`, `Round`, `Trunc`,
`Random`).

**Files** – whole files; errors are results:

```pascal
case WriteFile('notes.txt', 'one'#10'two'#10) of
  ok: WriteLn('saved');
  error e: WriteLn(e);                 { cannot open 'notes.txt': Permission denied }
end;
val lines := ReadLines('notes.txt');   { result array of String }
if lines is ok ls then for val l in ls do WriteLn(l);
```

`ReadFile(name): result String`, `ReadLines(name): result array of String`,
`WriteFile(name, text): result`, `AppendFile(name, text): result`, `DeleteFile(name): result`,
`FileExists(name): Boolean`.

Line by line with `TTextFile` (closed when the object is freed):

```pascal
if TTextFile.OpenRead('notes.txt') is ok f then
  loop
    begin
      if f.ReadLine is some line then WriteLn(line) else Break;
    end;
```

**Directories and paths:**

```pascal
if ForceDirs('out/logs') is error e then WriteLn(e);      { with all missing parents }
val r := ListDir('out');                                  { result array of String, sorted }
if r is ok names then
  for val n in names do
    if DirExists(JoinPath('out', n)) then WriteLn(n, '/') else WriteLn(n);
```

`ListDir(path): result array of String` (without `.` and `..`), `DirExists`, `CreateDir`,
`ForceDirs`, `RemoveDir` (empty directories), `RenameFile(old, new)`, `FileSize(name): result
Int64`, `CurrentDir`, `SetCurrentDir`, all with `result` errors like the file functions; and
for paths `ExtractFileName('src/main.pas') = 'main.pas'`, `ExtractFileDir` (`'src'`),
`ExtractFileExt` (`'.pas'`), `ChangeFileExt(path, '.c')`, `JoinPath('src', 'main.pas')`.

**Time:** `MilliSeconds: Int64` (monotonic, for measuring), `UnixTime: Int64`, `Sleep(ms)`,
`Now: TDateTime` (a record with `Year`, `Month`, `Day`, `Hour`, `Minute`, `Second`,
`MilliSecond`), `DateToStr`, `TimeToStr`, `DateTimeToStr` (`2026-11-25 14:05:09`).

**Program:** `ParamCount`, `ParamStr(i)` (0: the program), `GetEnv(name)` (`''` if not set).

* The C primitives behind these routines are declared `external` in the library; only the
  standard library may declare external routines.
* The containers own their elements; `Extract`, `Pop` and `Remove` pass the ownership to the
  caller (`optional T`), `Get` and `Peek` only borrow.
* Names of the program hide those of the library; the library itself keeps using its own.
* An unknown module gives `module 'SysUtils' not found (looked for ./SysUtils.pas)`.

## Records with methods

Records are **values**: `b := a` copies, value parameters and function results are copies,
records live directly in arrays, other records and classes (no allocation, no VMT, no
constructor). They can have methods and properties – like *advanced records* in Delphi or
`impl` for a `struct` in Rust:

```pascal
type
  TVector = record
    X, Y: Real;
    procedure Put(AX, AY: Real);
    procedure Scale(F: Real);                   { changes the record itself }
    function Len: Real;
    function Plus(B: TVector const): TVector;   { returns a new value }
    property Width: Real read X write X;
  end;

procedure TVector.Scale(F: Real); begin X := X * F; Y := Y * F; end;
function TVector.Plus(B: TVector const): TVector; begin Result.Put(X + B.X, Y + B.Y); end;
…
val
  a: TVector := (X: 3; Y: 4);
  b: TVector := default;
  points: array[1..100] of TVector := default;  { 100 values in a row, no allocation }
begin
  b := a;                               { copy }
  b.Scale(2);                           { a stays (3, 4) }
  WriteLn(a.Plus(b).Len:0:1);           { methods on result values too }
end.
```

* In record methods `Self` is the caller's record: methods can change it.
* Records have **no** inheritance, no virtual methods and no constructors or destructors; a
  literal, a procedure (`Put`) or a class method that returns the record
  (`class function TVector.Zero: TVector`) serves as "constructor".
* Records with methods must be declared as a named type.

Rule of thumb: **`record`** for small values that are copied (point, vector, date, colour),
**`class`** for objects with identity, inheritance or polymorphism. Where either lives is chosen
with `val` or `vao`.

## Borrow checker

For objects and references:

| Construct                      | Meaning                                           |
|--------------------------------|---------------------------------------------------|
| `vao a := TFoo.Create;`        | `a` **owns** the object                           |
| `var r: TFoo := a;`            | `r` is a **borrow** (owns nothing)                |
| `var r: TFoo const := a;`      | read-only borrow                                  |
| `var x: Integer := i;`         | borrow of the variable `i`                        |
| `Next: optional TItem;` (field) | the field owns the object (may be missing)       |
| `Parent: var TItem;` (field)   | the field is a borrow                             |
| `procedure P(x: TFoo)`         | parameter = borrow (`TFoo const`: read-only)      |
| `procedure P(vao x: TFoo)`     | the parameter takes over the ownership            |
| `procedure P(var x: TFoo)`     | changeable borrow of the owner's place            |
| `function F: TFoo`             | the result is handed to the caller                |
| `function F: var TFoo`         | the result is a borrow (e.g. `Find`)              |
| `Take(x.Next)`                 | takes the value out, the place becomes `none`     |

Rules (violations are **errors**, compilation stops):

* `b := a` between two owners is a **move**; `a` cannot be used afterwards
  (`use of moved value 'a'`) until it is assigned again.
* No access after `Free`/`Destroy` (`use after free`), no double free.
* Borrows of objects of local owners must not get into global variables, fields or function
  results – the owner frees the object at the end of its scope.
* A `var` result must not come from a temporary object (`F(TFoo.Create)`).
* Borrows must not be freed and not be stored in owners.
* A borrow remembers where it comes from. If the source is freed, moved or reassigned, the
  borrow is **dangling**; using it is an error.
* Functions must not return a reference to objects of local owners.
* The same variable must not be passed twice in one call if one of the parameters is
  `var`/`out` (`Swap(x, x)`).
* **Borrowing rules like in Rust:** at any time there are either **any number of read-only**
  (`var r: T const`) **or exactly one changeable** (`var r: T`) borrow of an object or a
  variable:
  - As long as a read-only borrow is still used, the object must not be changed – neither
    through the owner (field assignment, non-const method, `var` argument) nor through another
    borrow.
  - As long as a changeable borrow is still used, the object must not be used (not even read) or
    borrowed again in any other way.
  - As with Rust's *non-lexical lifetimes*, a borrow ends with its **last use**: a conflict is
    only an error if the borrow is used afterwards. The message names the place of the conflict:
    `borrowed reference 'l' is used here, but 'a' was changed at line 12 while 'l' was borrowing it`.
  - The rules are **field-precise**: a borrow of `a.Left` tolerates changes to `a.Right`.
    Borrows derived from another one (`p is some k`, `k.Next`) are compatible with it.
* You cannot simply move out of a field (`n := h.Next`) – that is what `Take` is for.
* `var` parameters must not be left freed/moved when the routine is left.
* **`var` fields** (references without ownership, `Parent: var optional TNode`) must not
  outlive the object they refer to:
  - Allowed when the holder is part of that object: `a.Child.Parent := a`, or in a method
    `c.Parent := Self` followed by storing `c` in `Self` (`Kids[i] := c`, `Child := c`).
  - Allowed between local owners of the same routine (or main program): after `b.Parent := a`
    the owner `b` *carries* a reference to `a`. If `a` is reassigned, freed or moved, `b` may not
    be used any more, and `a` must be declared before `b` so that it is freed after it:
    `'b.Parent' would refer to 'a', which is freed before 'b' (declare 'a' before 'b')`.
  - `b` may then move only into the object it refers to (`a.Add(b)`, `a.Child := b`) or into
    another local owner; storing a new reference in the field (`b.Parent := root`, `none`)
    replaces the old one.
  - Anything the compiler cannot follow is an error, e.g. a parameter stored in a field of
    another parameter (`c.Parent := p`): `cannot store the reference to 'p' in the 'var' field
    'c.Parent'`. Store such references from the owner's side instead (`p.Add(c)` with
    `c.Parent := Self` inside `Add`).

There are no memory leaks any more: whatever is still owned at the end of a scope is freed by
the generated code.

The analysis is flow-sensitive (if/case/loops with fixed-point iteration, `Break`/`Continue`/
`Exit`), understands `is some`, `x <> none`, `HasValue` and takes into account which non-local
variables called routines change (transitively).
Example: [tests/run/ownership.pas](tests/run/ownership.pas), error examples in
[tests/borrow_fail/](tests/borrow_fail/).

Limits of the analysis: it works per routine. Fields and dereferences are not tracked
individually (only their paths, to invalidate borrows); an object taken out of its parent
(`Take`) keeps its `var` references; global owners are only tracked in the main program; moving whole arrays/records that
own objects is secured at run time by emptying the source, not tracked by the checker.

## Optional values: `optional T`

There is no `nil`. A value that may be missing has the type **`optional T`** – for **every**
type, like `Option<T>` in Rust and `std::optional<T>` in C++. The empty value is called
**`none`**. An `optional` value cannot be used directly; it must be **unpacked** first:

```pascal
type
  TNode = class
    Value: Integer;
    Next: optional TNode;                { Option<Box<Node>> }
  end;

function Find(n: Integer): optional Integer;
begin
  if n > 0 then Result := n * 2          { Integer becomes some(...) automatically }
  else Result := none;
end;

val
  age: optional Integer := 42;           { std::optional<int> }
vao
  head: optional TNode := none;          { Option<Box<Node>> }
var
  p: optional TNode := none;             { Option<&Node> }
begin
  if age is some a then WriteLn(a);                { if let Some(a) = age }
  p := head;
  while p is some k do begin                       { while let Some(k) = p }
    Write(k.Value, ' ');
    p := k.Next;
  end;
  if (p is some k) and (k.Value > 10) then …       { binding + another condition }
  case Find(5) of                                  { match }
    some v: WriteLn('found ', v);
    none:   WriteLn('nothing');
  end;
  WriteLn(age.ValueOr(0));                         { unwrap_or / value_or }
  if age.HasValue then …                           { is_some / has_value }
  if age = none then …                             { is_none }
end.
```

Rules:

* **Unpacking:** `x is some v` (in `if` and `while` conditions, also combined with `and`),
  `case x of some v: … none: … end`, `x.ValueOr(d)` (for values), `x.HasValue`, `x = none` /
  `x <> none`. The variable `v` is valid in the `then` branch or the loop body. For objects `v`
  is a borrow; with `Take(x) is some v`, `v` takes over the ownership.
* `x.Field` or `x.Method` on an `optional` value is an error – with a hint how to unpack it.
  Exception: `x.Free` (frees nothing if `none`).
* A value of type `T` becomes `optional T` automatically; never the other way round.
* `none` only fits `optional` types; `x = none` for a non-optional `x` is an error.
* Optional objects are a pointer internally (`none` = NULL) and cost nothing extra; optional
  values are a small struct of a flag and the value.

For **non-optional** objects the compiler guarantees that they always have an object at run
time:

* **Owners without an object** do not exist: a non-optional `vao` must be initialized with an
  object, and using it after a move or `Free` is an error.
* **Constructors** must give all their own non-optional fields a value on every path before
  `Self` is used (method call, passing, storing). If a class inherits such fields, the
  constructor must call an inherited constructor. A class with non-optional fields must have a
  constructor of its own and must not be created with an inherited constructor that does not
  know its fields.
* **Memory that starts empty** (array elements, record fields of class type) must be
  `optional`.
* **Global variables and outer local variables:** calling a routine that reads a variable that
  is not set yet is an error (transitively through calls).
* `out` parameters must be set; functions with a non-optional result must deliver an object.
* `Take()` and `FreeAndNil()` are only allowed on `optional` places.

Because this guarantee is established at compile time, ppc produces **no** checks in the
program. With `--no-nil-check` the allocation analysis is switched off and every access through
a reference is checked at run time instead:

```
Runtime error 210: nil pointer dereference at program.pas(42,7)
```

A failed memory request (`Create`) ends with `Runtime error 203: heap overflow`.

**Array bounds** are always checked: a constant index outside the bounds is a compile-time error
(`index 7 is outside the array bounds 1..3`); a computed index is checked at run time (also for
strings, range 0..255):

```
Runtime error 201: range check error at program.pas(12,9): index 5 is outside 1..3
```

With `--no-range-check` the run-time check is dropped (faster, but without protection).

## Error handling: `result T`

There are no exceptions. A function that can fail returns **`result T`** – a value of type `T`
*or* an error, like `Result<T, E>` in Rust. The error is a `String`; with `result T of E` it is
of type `E` (e.g. an enumeration). `result` without `T` (or `result of E`) only reports success
or failure, like `Result<(), E>`.

```pascal
type TError = (DivByZero, TooBig);

function Divide(a, b: Integer): result Integer;
begin
  if b = 0 then Result := error('division by 0')
  else Result := a div b;                      { a value is ok(...) automatically }
end;

function Check(n: Integer): result of TError;
begin
  if n > 100 then Result := error(TooBig) else Result := ok;
end;

function Compute(a, b, c: Integer): result Integer;
begin
  val q := try Divide(a, b);                   { error: Compute returns it at once }
  begin
    Result := try Divide(q, c);
  end;
end;
…
case Divide(1, 0) of                           { match }
  ok v:    WriteLn('value ', v);
  error e: WriteLn('error: ', e);
end;
if r is ok v then WriteLn(v);                  { if let Ok(v) = r }
if Check(5) is ok then …;                      { result without a value }
WriteLn(Divide(7, 0).ValueOr(-1));             { unwrap_or }
if r.IsError then …;                           { is_err; also r.IsOk }
```

* **Creating:** `ok(v)`, `ok` (without a value), `error(e)`; a value of type `T` becomes
  `ok(...)` automatically, literals and `default` (= `ok` with the zero value) adapt.
* **Unpacking:** `case r of ok v: … error e: … end` (also with `else`; the expression is
  evaluated only once), `r is ok v` / `r is error e` in `if` and `while` conditions,
  `r.ValueOr(d)`, `r.IsOk`, `r.IsError`. Using a `result` directly as a value is an error – it
  cannot be skipped by accident.
* **Passing on with `try`** (like `?` in Rust): `try r` returns the value of a successful
  result; on an error the function ends at once and returns the error. Owners (`vao`) are freed
  as with `Exit`. `try` stands as a whole statement (`try Save(x);`), as the value of an
  assignment or as an initializer, and only in functions that themselves return a `result`
  with the same error type.
* The error is a value (number, string, enumeration, record without objects); the value can
  also be an **object** – see below.
* Internally a `result` is a small struct of an error flag, the value and the error – without
  cost as long as no error occurs.

A complete example (bank transfer with an error enumeration, a `try` chain and all kinds of
unpacking): [examples/transfer.pas](examples/transfer.pas).

### Objects in a result

A `result TFile` **owns** its object, like a `vao` variable:

```pascal
function Open(Name: String): result TFile;
begin
  if Name = '' then Result := error('no name')
  else Result := ok(TFile.Create(Name));      { ok(f) with 'vao f' moves f }
end;

if Open('a.txt') is ok f then f.Write('x');   { f takes the object; freed after the 'if' }
case Open('b.txt') of                         { the same in a case; freed after the case }
  ok f: f.Write('y');
  error e: WriteLn(e);
end;
vao f := try Open('c.txt');                   { in a function returning a result }

val r := Open('d.txt');                       { r owns the object ... }
if r is ok f then f.Write('z');               { ... f only borrows it }
```

* From a fresh result (a call) `is ok f` gives `f` the object, which is freed when the `if`
  (or `case`) ends; a result that is not taken (`is error e`, `is ok` without a name) frees its
  object at once. From a stored result `f` borrows; the borrow checker forbids changing `r`
  while `f` is used.
* `try` works on calls (`try Open(x)`), not on stored results, and `ValueOr` is not available –
  so an object always has exactly one owner.
* The standard library uses this for text files: `TTextFile.OpenRead(name)`, `OpenWrite`,
  `OpenAppend` return `result TTextFile` with `ReadLine: optional String`, `Write`,
  `WriteLine`, `EndOfFile`; the file is closed when the object is freed.

## Overloading: `overload`

Routines and methods (also constructors) may share a name if **each** variant is marked
`overload`, as in Delphi:

```pascal
function Show(i: Integer): String; overload;
begin Result := 'integer ' + IntToStr(i); end;
function Show(s: String): String; overload;
begin Result := 'string ' + s; end;
function Show(a, b: Integer): String; overload;
begin Result := 'two ' + IntToStr(a + b); end;

type
  TShape = class
    constructor Create; overload;
    constructor Create(AName: String); overload;
    function Describe: String; overload; virtual;
    function Describe(Prefix: String): String; overload;
  end;
  TCircle = class(TShape)
    function Describe: String; override;              { overrides the variant without parameters }
    function Describe(Times: Integer): String; overload; { a third variant }
  end;

WriteLn(Show(5), Show('hi'), Show(1, 2));
vao b := TShape.Create('box');
```

* The variants must differ in their **parameter types** (the result type and `var`/`vao` do
  not count): `"P(Integer)" is overloaded twice with the same parameters`.
* At a call the variant whose parameters fit the arguments best is chosen: the exact type
  first, then a widening (`Integer` to `Int64`, an object to its base class), then a conversion
  (`Integer` to `Real`, `Char` to `String`). If two variants fit equally well the call is an
  error: `the call of "P" with (Integer, Integer) is ambiguous; there are: P(Int64; Real), P(Real; Int64)`.
  If none fits, the message lists all variants.
* A descendant adds variants to those of its ancestors; `override` replaces the variant with
  the same parameters (it needs no extra `overload`). A method without `overload` hides the
  ancestors' methods of that name.
* The implementation of an overloaded method or forward routine is found by its parameters
  (`procedure TShape.Describe(Prefix: String)`), so it must repeat them.
* Overloads work in modules (interface headers), generic classes and for implementing
  interfaces. Interfaces themselves cannot overload, and an overloaded routine cannot be used
  as a function value (wrap the call in a lambda).

### Operators: `class operator`

Records can define operators (as in Delphi):

```pascal
type
  TVec = record
    X, Y: Real;
    class operator Add(a, b: TVec): TVec;
    class operator Multiply(a: TVec; f: Real): TVec;
    class operator Multiply(f: Real; a: TVec): TVec;   { several variants }
    class operator Negative(a: TVec): TVec;
    class operator Equal(a, b: TVec): Boolean;
  end;

class operator TVec.Add(a, b: TVec): TVec;
begin
  Result.X := a.X + b.X;
  Result.Y := a.Y + b.Y;
end;

c := a + b * 2 - a;           { the usual precedence }
if -a = c then …;
```

* Operators: `Add` `+`, `Subtract` `-`, `Multiply` `*`, `Divide` `/`, `IntDivide` `div`,
  `Modulus` `mod`, `Negative` `-x`, `Positive` `+x`, `LogicalNot` `not x`, `Equal` `=`,
  `NotEqual` `<>`, `LessThan` `<`, `LessThanOrEqual` `<=`, `GreaterThan` `>`,
  `GreaterThanOrEqual` `>=`.
* At least one operand has the record's type; comparisons return `Boolean`. Variants of one
  operator are chosen by the operand types like overloads (`a * 2` and `2 * a`).
* Without `NotEqual`, `<>` is `not Equal`; without `Equal`, `=` compares the fields. Only
  records (values) have operators – an operator on objects would create objects without an owner.

## Function values and lambdas

Procedural types describe routines that can be stored, passed and called; routines at program or
module level and **lambdas** (anonymous functions, as in Delphi) are their values:

```pascal
type
  TCompare = function(a, b: Integer): Integer;
  TAction = procedure(x: Integer);

function Desc(a, b: Integer): Integer; begin Result := b - a; end;

procedure ForEach(a: array of Integer; act: TAction);
begin
  for val i := 0 to High(a) do act(a[i]);
end;

val cmp: TCompare := Desc;                      { a routine as a value }
begin
  WriteLn(cmp(1, 5));
  ForEach(nums, procedure(x: Integer) begin Write(x * 10, ' '); end);
  SortBy(people, function(x, y: TPerson): Boolean begin Result := x.Age < y.Age; end);
end.
```

* Types can also be written directly: `Less: function(x, y: T): Boolean`; type inference of
  generics works through them (`SortBy`, `Filter` in `ppc_std`).
* A function value is called like a routine: `f(1, 2)`, also without arguments `t()` and on
  results: `Pick(True)(1, 5)`.
* A lambda that uses no variables of the routine around it is a plain function value (global
  variables can always be used). One that does is a **closure** – see below.
* Function values always refer to a routine: `default` is not allowed, and fields or array
  elements of a procedural type must be `optional` (e.g. an event `OnClick: optional procedure(n: Integer)`,
  called with `if b.OnClick is some h then h(3)`).
* Routines at program or module level are plain values, nested routines are closures; methods
  of an object are method values (below).
* The borrow checker treats a call through a function value as a call of any routine that is used
  as a value.

### Closures: `reference to function(...)`

A lambda inside a routine may use the routine's parameters, variables, nested routines and the
fields of `Self`. It refers to them (no copy): changes are visible on both sides.

```pascal
procedure ForEach(a: array of Integer; Visit: reference to procedure(x: Integer));
begin
  for val x in a do Visit(x);
end;

procedure Demo(Limit: Integer);
val Sum := 0;
begin
  ForEach(nums, procedure(x: Integer) begin Sum := Sum + x; end);      { Sum is changed }
  WriteLn(CountIf(nums, function(x: Integer): Boolean begin Result := x > Limit; end));
  val Twice: reference to function(x: Integer): Integer :=
    function(x: Integer): Integer begin Result := 2 * x + Sum; end;
  begin
    WriteLn(Twice(1));
  end;
end;
```

* Such a value has a **`reference to`** type (as in Delphi). Because it refers to the variables
  of a running routine, it may only be a **value parameter** or a **local variable** of a routine
  – not a field, array element, function result, `var` parameter, optional value, type argument
  or variable of the main program, and a closure variable can only be assigned in its own
  routine. So a closure never outlives the variables it uses; there is no allocation and no
  reference counting (the routine's frame is used, like for nested routines).
* A `reference to` parameter also accepts plain routines and lambdas; a closure for a plain
  procedural type is an error: `this lambda uses variables of the routine around it (a closure)
  and cannot be stored as "procedure(Integer)" … declare the parameter or variable 'reference to
  procedure(Integer)'`. `SortBy` and `Filter` in `ppc_std` take `reference to` parameters.
* Lambdas without parameters: `procedure begin … end`; a procedure value without parameters is
  called with `f;` or `f()`.
* The borrow checker knows what a closure uses: calling it after an object it uses was freed or
  moved is an error (`the call through a function value (a closure) uses 'k' after it was freed`),
  the same for nested routines.

### Method values: `obj.Method`

A method of an object can be passed where a `reference to` function is expected; it is called
on that object (virtual methods in the object's class, also through interfaces and on records):

```pascal
ForEach(nums, printer.Print);         { printer.Print(x) for every element }
SortBy(names, cmp.Less);              { a const method as the comparison }
Twice(counter.Tick);                  { a method without parameters: not called here }
```

* A method value can only be **passed directly as an argument**: during the call its object
  surely exists. To keep it in a variable, use a lambda that calls the method
  (`val f := procedure begin counter.Tick; end`) – the closure rules then apply.
* The object must be a variable (not a temporary object), a read-only object only gives `const`
  methods, and the borrow checker forbids moving or `var`-passing the object in the same call.
  Overloaded methods, class methods and constructors cannot be values.

## Enumerations with data

An enumeration whose values carry data (a sum type, like `enum` in Rust). It is written like a
Pascal enumeration; variants with data list their fields in parentheses:

```pascal
type
  TShape = (
    Circle(Radius: Real),
    Rect(W, H: Real),
    Named(Name: String; Sides: Integer),
    Empty
  );

val s := Circle(2.0);                    { or TShape.Circle(2.0) }
s := Empty;
```

The fields are read with `case` or `if … is …`. The names in a pattern are **references** to
the fields (read-only; `var n` makes a changeable one), `_` skips a field:

```pascal
function Area(s: TShape): Real;
begin
  case s of
    Circle(r): Result := 3.14159 * r * r;
    Rect(w, h): Result := w * h;
    Named(_, n): Result := n;
    Empty: Result := 0;
  end;
end;

if s is Named(n, k) then WriteLn(n, ' ', k) else WriteLn('not named');
if s is Empty then ...                   { a test without names: an ordinary Boolean }
case s of Named(var n, _): n := n + '!'; else end;
```

* `case` must handle **every** variant or have an `else` (`case does not handle the variants
  Rect, Empty of "TShape"`); an `else` after all variants is an error, a variant handled twice
  too.
* A variant can be named without its fields (`Circle: …`); several variants without names can
  share an arm (`Rect, Empty: …`).
* A pattern with names is only allowed as the whole condition of an `if`, otherwise use `case`.
* Fields can be of any type; objects must be `optional T` as in records. The value owns its data:
  strings and objects are freed when the value is overwritten or goes out of scope; assigning a
  value with objects moves it.
* The borrow checker knows the bindings: changing `s` while a binding is still used is an error.
* Generic: `TMaybe<T> = (Just(Value: T), Nothing)`, built with `TMaybe<Integer>.Just(5)`.
* `default` is the first variant with zero fields. `=` and `<>` compare the variant and its fields:
  `s = Circle(2.0)`.

Internally the value is a record with a tag and the fields of all variants.

## Loops over collections: `for val x in …`

`for val x in c do …` runs through the elements of an array (fixed or dynamic), a string (the
characters), a container with the methods `Count` and `Get(i)` (such as `TList<T>`) or a type with
`GetEnumerator` (below). `x` is a
**reference** to the element, nothing is copied:

* `for val x in c` – read-only: `x := …`, `x.Field := …` and changing methods are errors.
* `for var x in c` – changeable: assignments change the element in the collection.

```pascal
val a: array[1..4] of Integer := (3, 1, 4, 1);
for var x in a do x := x * 10;               { a = (30, 10, 40, 10) }
for val c in 'abc' do Write(c, '-');         { a-b-c- }

vao people := TList<TPerson>.Create;
...
for val p in people do WriteLn(p.Name);      { optional elements: empty ones are skipped }
```

Optional elements (`optional T`, as returned by `TList<T>.Get`) are unpacked; empty elements are
skipped. The loop is an index loop underneath, so it is as fast as `for i := … to …`. The
borrow checker knows that `x` borrows the collection: shrinking it with `SetLength` or changing
the container (`people.Clear`) while `x` is still used is an error. `for x in c` without `val` or
`var` is an error.

### Own iterators: `GetEnumerator`

A type can be used in `for … in` by providing `GetEnumerator` (as in Delphi). The enumerator – a
record or an object – has `function MoveNext: Boolean` and `Current` (a function or a property):

```pascal
type
  TRangeEnum = record
    Cur, Hi: Integer;
    function MoveNext: Boolean;              { Inc(Cur); Result := Cur <= Hi }
    property Current: Integer read Cur;
  end;
  TRange = record
    Lo, Hi: Integer;
    function GetEnumerator: TRangeEnum; const;
  end;

for val i in Range(1, 10) do Write(i, ' ');

  TBagEnum = record
    Bag: var optional TBag;                  { refers to its container }
    I: Integer;
    function MoveNext: Boolean;
    function Current: Integer; const;
  end;

function TBag.GetEnumerator: TBagEnum;
begin
  Result.I := -1;
  Result.Bag := Self;                        { allowed in GetEnumerator }
end;

for val x in bag do Write(x, ' ');
```

* An enumerator object is owned by the loop and freed after it; an optional `Current` skips
  empty elements, a `var` result of `Current` gives a reference (`for var x in …`), otherwise
  only `for val` is possible.
* `GetEnumerator` may store `Self` in a `var` field of its result; such an enumerator can only be
  used by `for … in`. While the loop runs, the collection is borrowed read-only: changing it in
  the loop body is an error (`the collection of this 'for ... in' loop must not change while the
  loop runs`).
* `GetEnumerator` takes precedence over `Count`/`Get(i)`.

## Endless loop: `loop`

`loop <statement>` repeats the statement without a condition (like `loop` in Rust); it is left
with `Break`, `Exit` or a failed `try`:

```pascal
loop
  begin
    ReadLn(line);
    if line = 'end' then Break;
    Process(line);
  end;

loop
  vao k := TJob.Create(n);           { a fresh owner in every iteration }
  begin
    if not k.Run then Break;         { k is freed on Break too }
  end;
```

If `Break` or `Continue` leaves a block with its own variables, its owners free their objects at
once (in every kind of loop); borrows of them are invalid afterwards.

## Supported language

* `program`, `module` and `import` (own modules, standard library `ppc_std`), `const` (also
  typed constants and array initializers), `type`, `val` (values), `var` (references), `vao`
  (owners), blocks with their own sections
* Types: `Integer`/`LongInt` (32 bit), `ShortInt`, `SmallInt`, `Byte`, `Word`, `Cardinal`,
  `Int64`, `Boolean`, `Char`, `Real`/`Double`, `String` (dynamic, any length),
  enumerations (also with data), subranges, sets (`set of T`), static (also multi-dimensional) and dynamic arrays, records (with
  methods), `class` (with class methods), `interface`, generics
* Procedures/functions, nested to any depth (access to outer variables through static links),
  `var`, `out`, `vao` and `T const` parameters, `forward`, recursion, `Result`/function name,
  `Exit(value)`
* Statements: `:=`, `if`, `case` (also with strings and ranges), `while`, `repeat`,
  `for … to/downto`, `for val/var x in …`, `loop` (endless loop), `Break`, `Continue`, `Exit`
* Expressions: arithmetic, `div`, `mod`, `shl`, `shr`, `and`/`or`/`xor`/`not` (short-circuit for
  Boolean), comparisons, string concatenation, `in [a, b..c]`, type casts `Integer(x)`,
  `TFoo(obj)`
* Standard routines: `Write(Ln)` with `:width:decimals`, `Read(Ln)`, `Length`, `Copy`, `Pos`,
  `Delete`, `Insert`, `UpCase`, `UpperCase`, `LowerCase`, `Concat`, `Format`, `IntToStr`, `StrToInt`,
  `FloatToStr`, `Hash`, `Str`, `SetLength`, `Ord`, `Chr`, `Succ`, `Pred`, `Inc`, `Dec`, `Odd`, `Abs`,
  `Sqr`, `Sqrt`, `Sin`, `Cos`, `ArcTan`, `Exp`, `Ln`, `Trunc`, `Round`, `Frac`, `Int`, `High`,
  `Low`, `SizeOf`, `Random`, `Randomize`, `Halt`, `Assigned`, `FreeAndNil`, `Take`;
  for `optional`: `HasValue`, `ValueOr`; for `result`: `IsOk`, `IsError`, `ValueOr`

Not supported: `nil` (replaced by `optional`/`none`), pointers (`^`, `@`, `New`, `Dispose`,
`Pointer` – replaced by objects, `var` references and `var` parameters), `object` types
(replaced by `class`), operators on classes, open array parameters
(`array of T` is a dynamic array), closures that outlive their routine, `with` (name the variable, or take a short reference
`var q := a.Inner`), exceptions (replaced by `result T` and `try`), reference-counted
interfaces, variant records (replaced by enumerations with data), `goto`, Pascal file types (`TextFile`,
`Assign`/`Reset`; whole-file I/O is in `ppc_std`).

## Structure

| File              | Contents                                                   |
|-------------------|------------------------------------------------------------|
| `src/parser.c`    | lexer (case-sensitive) and recursive-descent parser        |
| `src/sema.c`      | name resolution, type checking, class layout, VMT slots, generics |
| `src/borrow.c`    | borrow checker and allocation/initialization analysis      |
| `src/codegen.c`   | C code generation (structs, VMTs, static links, runtime calls) |
| `src/ppc_rt.h`    | C runtime library, embedded into every program             |
| `src/main.c`      | driver, prelude (`TObject`), modules, calling the C compiler |
| `lib/ppc_std.pas` | standard library in Perfect Pascal, embedded               |
| `tests/`          | test programs and `run.sh`                                 |
| `examples/`       | example programs                                           |

Classes are mapped to C structs with a `__vmt` pointer at the start. Inherited fields come first
as a prefix, so pointers to descendants are layout-compatible with pointers to ancestors. A heap
object is a struct allocated with `calloc`; a stack object is the same struct as a local C
variable that the Pascal variable points to, and at the end of the block the generated code
calls the virtual destructor without freeing. At the end of a scope the generated code frees
everything that `vao` variables still own (in reverse order). A `var` reference to a value is a
pointer to the original variable in C; class methods are C functions without a `self`
parameter.
