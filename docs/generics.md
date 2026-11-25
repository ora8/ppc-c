# Design: generics for ppc

Status: **accepted** (all proposals of section 8 accepted). Stage 1 (generic records and
classes), stage 2 (generic functions and procedures), stage 3 (constraints), stage 4 (checking
the body against the constraints), stage 5 (dynamic arrays) and stage 6 (standard library
`import ppc_std`) are built – all stages are done.

## 1. Goal

Types and routines should be written once and be usable for any type – checked at compile time,
without `as` casts and as fast as hand-written code.

```pascal
vao customers := TList<TCustomer>.Create;
customers.Add(TCustomer.Create('Anna'));
if customers.Get(0) is some k then WriteLn(k.Name);   { k is a TCustomer, not a TObject }

WriteLn(Max(3, 7), ' ', Max('Anna', 'Bert'));          { Max<Integer>, Max<String> }
```

**Non-goals:** no template metaprogramming as in C++, no specialization for single types, no
type parameters that are values (`array[1..N]` with a generic `N`).

## 2. Syntax

In the style of Delphi / Free Pascal (`<...>` after the name):

```pascal
type
  TPair<A, B> = record                         { generic record }
    First: A;
    Second: B;
  end;

  TList<T> = class                             { generic class }
    Items: array[1..100] of optional T;
    Size: Integer;
    procedure Add(vao x: T);
    function Get(i: Integer): var optional T;
    function Len: Integer; const;
  end;

  TCustomerList = class(TList<TCustomer>)      { inherits from an instance }
    function Find(Name: String const): var optional TCustomer;
  end;

procedure TList<T>.Add(vao x: T);              { methods repeat <T> }
begin
  Inc(Size);
  Items[Size] := x;                            { move, even if T is an object }
end;

function Max<T: Comparable>(a, b: T): T;       { generic function with a constraint }
begin
  if a > b then Result := a else Result := b;
end;
```

Use:

```pascal
val p: TPair<String, Integer> := (First: 'Anna'; Second: 30);
vao l := TList<TCustomer>.Create;
val m := Max(3, 7);                            { T from the arguments: Integer }
val r := Max<Real>(3, 7.5);                    { T written out }
```

**Ambiguity of `<`:** in expressions, `Max<Real>(…)` could also be read as the comparison
`Max < Real …`. After an identifier the parser therefore first tries to read `< type list >`
followed by `(` or `.`, and otherwise falls back to the comparison (the parser works on a token
array, so going back is cheap). In types there is no ambiguity.

## 3. Constraints

Without a constraint you may only do with a `T` what works for **every** type: store, move,
borrow, pass, wrap it in `optional T` / `array of T` / `result T`. Everything else must be
announced:

| Constraint | allowed in the body | fitting types |
|---|---|---|
| (none) | store, move, borrow | all |
| `T: class` | `T` is an object: `ClassName`, `is`, `as`, `Free` | all classes |
| `T: TBase` | all fields and methods of `TBase` | `TBase` and its descendants |
| `T: Copy` | `b := a` copies, `a` stays valid | values without objects (numbers, strings, simple records) |
| `T: Clone` | `x.Clone` | classes/records with `Clone`, arrays of them |
| `T: Equatable` | `=`, `<>` | numbers, strings, Char, Boolean, enumerations |
| `T: Comparable` | also `<`, `<=`, `>`, `>=` | numbers, strings, Char, enumerations |
| `T: Numeric` | `+`, `-`, `*`, number literals, comparisons | integer types, Real |

Several constraints: `T: Clone, Comparable`; parameter groups with constraints are separated by
`;` as in Delphi: `TMap<K: Comparable; V>`. The constraints are checked at every instantiation:
`TList<TCustomer>` with `T: Comparable` gives
`"TCustomer" does not satisfy 'T: Comparable' (required by TList<T>)`.

## 4. Ownership and the borrow checker

This is the part where ppc differs from Delphi. Principle (as in Rust): **a `T` without `Copy`
is always treated like an owner in the generic body.**

```pascal
procedure TList<T>.Swap(i, j: Integer);
begin
  val h := Take(Items[i]);          { ok: Take works for optional T }
  Items[i] := Take(Items[j]);
  Items[j] := h;                     { move }
end;

function MoveTwice<T>(a: T): TPair<T, T>;
begin
  Result := (First: a; Second: a);   { error: a would be moved twice – 'T: Copy' or 'T: Clone' needed }
end;
```

* Assigning a `T` is a **move**; after the move the source is dead. With `T: Copy` it is a copy.
  So the body is correct for objects **and** values, and the borrow checker reports errors in
  the generic code, not only for a particular instance.
* `vao x: T` takes over the ownership, `var x: T` is a reference, `x: T` borrows – exactly as
  for normal parameters.
* **Storage of `val x: T`:** in generic code `val x: T` means "owns a T in place". For values
  the value lives there, for objects an owning reference (like `vao`). So no stack objects are
  created in generic code.
* Fields of type `T` own their content; for objects the field must be `optional T`, as for
  records today (memory starts empty). The compiler requires this already in the generic type.
* Automatic freeing, moves of arrays/records with objects and `Clone` work unchanged, because
  they are generated per instance.

## 5. Checking and translation

**Monomorphization:** for every type combination used, the compiler creates its own copy of the
type or the routine – like Rust and C++. `TList<TCustomer>` and `TList<Integer>` become two C
structs with their own functions and their own VMT. This costs nothing at run time.

* Names in the C code: `struct TList_TCustomer12` (the name with its number; other characters become `_`); `ClassName` returns
  `'TList<TCustomer>'`.
* Equal instances are created only once (key: name + type arguments).
* Recursive instances (`TTree<T>` with a field `optional TTree<T>`) work through the existing
  forward declarations.
* Unlimited nesting (`TList<TList<TList<...>>>`) is stopped after 50 levels.

**Type checking** – two possibilities:

1. **Per instance** (like C++ templates): the body is checked with the actual types. Simple to
   build, uses the existing type checking almost unchanged. Drawback: errors in generic code
   only appear when it is used, and the message names the instance.
2. **Against the constraints** (like Rust): the body is checked once with an "abstract" `T`;
   every use of `T` that no constraint allows is an error at once. Better messages, but much
   more work in the type checker.

**Implementation:** (1) since stage 1; (2) since stage 4: every generic is additionally checked
once with placeholder types that can do exactly what their constraints allow (with a base class
as constraint the placeholder is that class). No code is produced for this; a generic with
errors is not reported again for its instances. The borrow checker also runs on these placeholder
bodies: a placeholder without `Copy`, `Comparable` or `Numeric` counts as an object (moves,
no copying out of containers); a generic whose body has borrow errors is reported once and its
instances are not checked again.

## 6. Prerequisite: growing arrays

Generic containers are only half useful with `array[1..100] of optional T` – the size is fixed.
`TList<T>`, `TStack<T>` and `TMap<K, V>` need memory that can grow. Two ways:

* **Dynamic arrays** `array of T` with `SetLength(a, n)`, `Length(a)`, index checks and
  automatic freeing (owns its elements like a fixed array today).
* or only a built-in generic class `TVec<T>` (like Rust's `Vec<T>`) on which everything else is
  built.

**Decision:** dynamic arrays `array of T` as a stage of their own before the containers.

## 7. Stages

| Stage | Contents | Result |
|---|---|---|
| 1 | generic records and classes: type parameters, fields, methods, explicit instantiation `TList<TCustomer>`, inheritance from instances, checking per instance, monomorphization | `TPair<A, B>`, `TList<T>` with a fixed array |
| 2 | generic functions and procedures, type inference from the arguments | `Max(3, 7)`, `Swap<T>(var a, b: T)` |
| 3 | constraints `class`, base class, `Copy`, `Clone`, `Equatable`, `Comparable`, `Numeric` | `Sort<T: Comparable>` |
| 4 | checking the generic body against the constraints (errors without an instance) | Rust-like messages |
| 5 | dynamic arrays `array of T` | growing containers |
| 6 | small standard library: `TList<T>`, `TStack<T>`, `TMap<K, V>`, `Sort`, `IndexOf` | as `import ppc_std` |

Every stage comes with its own tests (run and error tests), an ASan check and a README section.

## 8. Decisions

| Question | Decision |
|---|---|
| Syntax | `TList<T>` like Delphi |
| `val x: T` for object instances | owning reference like `vao`; no stack objects in generic code |
| Checking strategy | first per instance, from stage 4 against the constraints |
| Order | dynamic arrays as a stage of their own before the containers |
| Standard library | as Perfect Pascal source (`import ppc_std`) |

Original questions:

1. **Syntax:** `TList<T>` like Delphi (proposal) or `TList[T]` (no ambiguity with `<`)?
2. **`val x: T` for object instances:** owning reference like `vao` (proposal), or allow only
   `vao`/`var` for `T` in generic code?
3. **Checking strategy:** first per instance, later against the constraints (proposal) – or
   Rust-like right away?
4. **Order:** dynamic arrays before the generics, after them or in parallel?
5. **Standard library:** as Perfect Pascal source (`import ppc_std`) or built into the compiler
   like `TObject`?
