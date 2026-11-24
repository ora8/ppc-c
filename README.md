# ppc – Perfect Pascal Compiler

`ppc` (**Perfect Pascal Compiler**) übersetzt einen objektorientierten Pascal-Dialekt (Turbo Pascal / Delphi / Free Pascal)
nach **C** und ruft danach `gcc` auf, um ein natives Programm zu erzeugen.
Der Compiler ist selbst in C geschrieben (C11, keine Abhängigkeiten).

Besonderheiten:

* **Groß-/Kleinschreibung wird unterschieden** (anders als Standard-Pascal).
* **Stack- und Heapobjekte derselben Klasse:** Eine Klassenvariable im Abschnitt `val` ist
  ein **Stackobjekt** (automatisches `Destroy` am Blockende), im Abschnitt `var` ein
  **Heapobjekt** (`Create`/`Free` von Hand).
* **Borrow Checker** nach dem Vorbild von Rust: prüft Besitz (Ownership) und Ausleihen (Borrows)
  zur Compile-Zeit.
* **Keine Zeiger und kein nil:** Es gibt kein `^`, `@`, `New`, `Dispose` und kein `nil`.
  Werte, die fehlen können, haben den Typ **`optional T`** (wie `Option<T>` in Rust bzw.
  `std::optional<T>` in C++) und müssen vor der Benutzung ausgepackt werden. Nicht allozierte
  Heapobjekte, nicht konstruierte Stackobjekte und Konstruktoren, die Felder vergessen,
  werden beim Übersetzen erkannt.

## Bauen und benutzen

```bash
make            # erzeugt ./ppc
make test       # Testsuite (Ausgaben, Borrow-/Optional-/Stackobjekt-Fehler, Compile-Fehler)
```

```bash
./ppc programm.pas            # erzeugt ./programm
./ppc -o hallo programm.pas   # Name des Programms
./ppc -S programm.pas         # nur C-Code erzeugen (programm.c)
./ppc -k programm.pas         # C-Datei zusätzlich behalten
./ppc --no-borrow-check x.pas # Borrow Checker abschalten
./ppc --no-nil-check x.pas    # Allokationsanalyse aus, stattdessen Prüfung zur Laufzeit
./ppc --cc clang -O3 x.pas    # anderer C-Compiler / Optimierung
```

## Groß-/Kleinschreibung

* Schlüsselwörter werden **klein** geschrieben: `program begin end if then class val var …`
  (`Begin` ist ein Bezeichner, kein Schlüsselwort).
* Bezeichner sind case-sensitiv: `Count` und `count` sind zwei verschiedene Namen.
* Vordefinierte Namen in Delphi-Schreibweise: `Integer`, `Boolean`, `Char`, `Real`, `String`,
  `Byte`, `Word`, `Int64`, `Cardinal`, `True`, `False`, `MaxInt`, `Pi`,
  `WriteLn`, `Write`, `ReadLn`, `Read`, `Length`, `Ord`, `Chr`, `Inc`, `Dec`,
  `TObject`, `Create`, `Destroy`, `Free`, `ClassName`, `Self`, `Result`, `Exit`, `Break`, `Continue` usw.
* Direktiven/Modifizierer klein: `virtual`, `override`, `abstract`, `forward`, `private`,
  `public`, `property … read … write …`, `borrowed`, `owned`, `optional`, `some`, `none`.

## Klassen

Referenztypen, implizit von `TObject` abgeleitet:
`constructor Create`, `destructor Destroy; override;`, `Free`, `ClassName`,
`virtual`/`override`/`abstract`, `inherited`, Properties mit Feld- oder Methoden-Zugriff,
`is`/`as`, `Self`, Sichtbarkeitsabschnitte (werden geparst, nicht erzwungen), Forward-Klassen
(`TFoo = class;`).

## Stack- und Heapobjekte

Es gibt **nur eine Art von Klasse**. Wo ein Objekt liegt, entscheidet der Abschnitt, in dem
die Variable deklariert ist. Neben `const` und `type` gibt es zwei Variablenabschnitte:
**`val`** für Werte und Stackobjekte und **`var`** für alles, was auf dem Heap liegt. `var` bei Parametern
(`procedure P(var x: Integer)`) bedeutet weiterhin Übergabe per Referenz.

| Deklaration | Bedeutung |
|---|---|
| `val k: TKreis;` | **Stackobjekt**: das Objekt liegt im Stackframe (global: im Datensegment) |
| `var h: TKreis;` | **Heapobjekt**: `h` besitzt ein Objekt auf dem Heap |
| `var n: optional TKreis;` | Heapobjekt, das auch fehlen darf (`none`) |
| `val r: borrowed TKreis;` | Ausleihe: Referenz ohne Besitz auf ein Stack- oder Heapobjekt |
| `val i: Integer;` | gewöhnlicher Wert (Zahlen, Strings, Records, Arrays, `optional`-Werte) |
| `var I: Integer;` | **Heapwert**: Integer (oder String, Record, Array …) auf dem Heap, siehe unten |

```pascal
procedure Zeige(F: TFigur);         { nimmt Stack- und Heapobjekte }

procedure Demo;
val
  k: TKreis;                        { Stackobjekt }
var
  h: TFigur;                        { Heapobjekt }
begin
  k.Create(1, 2, 3.0);              { Stack: nur Konstruktor, kein malloc }
  h := TKreis.Create(4, 5, 1.0);    { Heap: Speicher holen + Konstruktor }
  Zeige(k);
  Zeige(h);                         { gleicher Typ, virtuelle Methoden, Polymorphie }
  h.Free;                           { Heap: von Hand freigeben }
end;                                { Stack: k.Destroy wird automatisch aufgerufen }
```

**Erzeugen direkt bei der Deklaration:** Statt eines Typs darf `Typ.Konstruktor(...)` stehen.
Ein Heapobjekt wird dann sofort erzeugt, ein Stackobjekt sofort konstruiert – es ist also nie
„leer“:

```pascal
procedure P(n: Integer);
val
  s: TFoo.Create(n);          { Stackobjekt, sofort konstruiert }
  t: TFoo.Leer;               { anderer Konstruktor, ohne Argumente }
var
  h: TFoo.Create(n * 10);     { Heapobjekt, sofort erzeugt (≙ var h: TFoo = TFoo.Create(n * 10)) }
  I: Integer.Create(n + 1);   { Heapwert }
```

Die Argumente dürfen beliebige Ausdrücke mit Variablen sein. Sie werden **einmal** ausgewertet,
wenn die Deklaration ausgeführt wird: beim Eintritt in die Routine bzw. in den Block (siehe
„Blöcke“). Spätere Änderungen der Variablen ändern das Objekt nicht mehr; wer einen Wert aus
dem Programmablauf braucht, deklariert das Objekt in einem Block an der passenden Stelle.

* Klasse, Methoden, Vererbung und VMT sind identisch – nur der Speicherort unterscheidet sich.
* **Stackobjekte** werden mit `k.Create(...)` konstruiert. Der Compiler verlangt das vor der
  ersten Benutzung (auf jedem Pfad) und genau einmal. Am Blockende wird `Destroy`
  automatisch aufgerufen (umgekehrte Deklarationsreihenfolge, globale Stackobjekte am
  Programmende). `k.Free`, `k := …` und Referenzen auf `k`, die den Block verlassen
  (globale Variable, Feld, Funktionsergebnis), sind Fehler.
* **Heapobjekte** werden **nicht automatisch** freigegeben. Der Borrow Checker verfolgt den
  Besitz und warnt bei Speicherlecks; Benutzung nach `Free`, doppeltes `Free` usw. sind Fehler.
* Felder, Parameter und Funktionsergebnisse vom Klassentyp sind Referenzen (siehe unten).
* Für Werte mit Kopiersemantik gibt es `record` (auch mit Methoden, siehe unten).

### Heapwerte: Werte im Abschnitt `var`

Auch Zahlen, Strings, Records und Arrays können auf dem Heap liegen (wie `Box<T>` in Rust).
Sie werden im Abschnitt `var` deklariert, beim ersten Speichern eines Werts (oder mit
`T.Create`) angelegt und mit `.Free` freigegeben; dazwischen benutzt man sie wie einen
normalen Wert:

```pascal
var
  I: Integer;
  V: TVektor;                  { Record }
begin
  I := 20;                     { erster Wert: legt die Heap-Zelle an }
  I := I + 1;
  Verdopple(I);                { var-Parameter: Referenz auf den Heapwert }
  V := TVektor.Create;         { oder mit Anfangswert: T.Create(wert) }
  V.Skaliere(2);
  I.Free;
  V.Free;
end;
```

* Die erste Speicherung legt die Zelle an (`I := 20`, auch nach `Free` wieder);
  `I := T.Create` / `T.Create(wert)` tut das ausdrücklich. Jede weitere Zuweisung speichert
  einen neuen **Wert** in der Zelle; `J := I` zwischen zwei Heapwerten kopiert also den Wert.
* Lesen, bevor ein Wert gespeichert wurde, Benutzung nach `Free` und doppeltes `Free` sind
  Fehler; ein fehlendes `Free` gibt eine Leck-Warnung.
* Für einzelne Zahlen lohnt sich das selten; nützlich ist es für große Records oder Arrays,
  die nicht im Stackframe liegen sollen.

## Typ aus der Zuweisung und Blöcke

**Variablen ohne Typangabe** bekommen ihren Typ vom Initialisierer oder von der ersten
Zuweisung im Code. Danach ist der Typ fest:

```pascal
val
  N;                     { Typ kommt aus der ersten Zuweisung }
  S = 'Hallo';           { String, aus dem Initialisierer }
var
  I;                     { Heapwert, Typ aus der ersten Zuweisung }
begin
  N := 3.5;              { N ist jetzt Real }
  I := 5;                { I ist jetzt ein Integer auf dem Heap }
  N := 'x';              { Fehler: N ist Real }
  I.Free;
end;
```

* Wird die Variable vor der ersten Zuweisung gelesen oder nie zugewiesen, ist das ein Fehler.
* Aus `none` lässt sich kein Typ ableiten (`val x: optional T` schreiben).
* `val k = andererObjekt` ergibt eine Ausleihe; ein Stackobjekt braucht einen Typ
  (`val k: TFoo;` und `k.Create(...)`), ein Heapobjekt geht mit `var k = TFoo.Create(...)`.

**Blöcke mit eigenen Variablen:** Vor einem verschachtelten `begin … end` dürfen `val`-,
`var`- und `const`-Abschnitte stehen. Sie bilden einen eigenen Gültigkeitsbereich – auch als
Schleifenrumpf, dann mit frischen Variablen in jedem Durchlauf:

```pascal
begin
  val
    j = 10;
    s: TFoo;             { Stackobjekt nur in diesem Block }
  begin
    s.Create(1);
    WriteLn(j, ' ', s.V);
  end;                   { s.Destroy wird hier aufgerufen, j ist hier nicht mehr sichtbar }

  while i < 3 do
    val q = i * 100;
    begin
      WriteLn(q);
      Inc(i);
    end;
end;
```

* Stackobjekte werden am Blockende abgebaut; Ausleihen auf sie sind danach ungültig
  (der Borrow Checker meldet ihre Benutzung). Heapobjekte, die am Blockende noch besessen
  werden, ergeben eine Leck-Warnung.
* Routinen, Typen und typisierte Konstanten sind in Blöcken nicht erlaubt.

## Records mit Methoden

Records sind **Werte**: `b := a` kopiert, Wertparameter und Funktionsergebnisse sind Kopien,
Records liegen direkt in Arrays, anderen Records und Klassen (ohne Allokation, ohne VMT, ohne
Konstruktor). Sie können Methoden und Properties haben – wie *Advanced Records* in Delphi oder
`impl` für ein `struct` in Rust:

```pascal
type
  TVektor = record
    X, Y: Real;
    procedure Setze(AX, AY: Real);
    procedure Skaliere(F: Real);                { ändert den Record selbst }
    function Laenge: Real;
    function Plus(const B: TVektor): TVektor;   { liefert einen neuen Wert }
    property Breite: Real read X write X;
  end;

procedure TVektor.Skaliere(F: Real); begin X := X * F; Y := Y * F; end;
function TVektor.Plus(const B: TVektor): TVektor; begin Result.Setze(X + B.X, Y + B.Y); end;
…
val
  a, b: TVektor;
  punkte: array[1..100] of TVektor;     { 100 Werte hintereinander, keine Allokation }
begin
  a.Setze(3, 4);
  b := a;                               { Kopie }
  b.Skaliere(2);                        { a bleibt (3, 4) }
  WriteLn(a.Plus(b).Laenge:0:1);        { Methoden auch auf Ergebniswerten }
end.
```

* In Record-Methoden ist `Self` der Record des Aufrufers: Methoden können ihn ändern.
* Records haben **keine** Vererbung, keine virtuellen Methoden und keine Konstruktoren oder
  Destruktoren; als „Konstruktor“ dient eine Prozedur (`Setze`, `Init`) oder eine Funktion,
  die den Record zurückgibt.
* Records mit Methoden müssen als benannter Typ deklariert werden.

Faustregel: **`record`** für kleine Werte, die kopiert werden (Punkt, Vektor, Datum, Farbe),
**`class`** für Objekte mit Identität, Vererbung oder Polymorphie.

## Borrow Checker

Für Werte von **Klassentypen** gilt:

| Konstrukt                      | Bedeutung                                         |
|--------------------------------|---------------------------------------------------|
| `var a: TFoo;`                 | `a` **besitzt** ein Heapobjekt (Owner)            |
| `val r: borrowed TFoo;`        | `r` ist eine **Ausleihe** (besitzt nichts)        |
| `val s: TFoo;`                 | Stackobjekt (gehört dem Block)                    |
| `Next: optional TItem;` (Feld)  | Feld besitzt das Objekt (darf fehlen)             |
| `Parent: borrowed TItem;`      | Feld ist eine Ausleihe                            |
| `procedure P(x: TFoo)`         | Wert-/`const`-Parameter = Ausleihe                |
| `procedure P(owned x: TFoo)`   | Parameter übernimmt den Besitz                    |
| `procedure P(var x: TFoo)`     | veränderliche Ausleihe des Besitzer-Platzes       |
| `function F: TFoo`             | Ergebnis wird an den Aufrufer übergeben           |
| `function F: borrowed TFoo`    | Ergebnis ist eine Ausleihe (z. B. `Find`)         |
| `Take(x.Next)`                 | holt den Wert heraus, der Platz wird `none`       |

Regeln (Verstöße sind **Fehler**, die Übersetzung bricht ab):

* `b := a` zwischen zwei Besitzern ist ein **Move**; `a` ist danach unbenutzbar
  (`use of moved value 'a'`), bis es neu zugewiesen wird.
* Kein Zugriff nach `Free`/`Destroy` (`use after free`), kein doppeltes Freigeben.
* Ausleihen dürfen nicht freigegeben und nicht in Besitzer gespeichert werden.
* Eine Ausleihe merkt sich, woher sie stammt. Wird die Quelle freigegeben, verschoben oder
  neu zugewiesen, ist die Ausleihe **hängend**; ihre Benutzung ist ein Fehler.
* Referenzen auf Stackobjekte (`val s: TFoo`) dürfen nicht in globale
  Variablen, Felder oder Funktionsergebnisse entkommen; Funktionen dürfen keine Referenz auf
  Lokales zurückgeben.
* Dieselbe Variable darf in einem Aufruf nicht zweimal übergeben werden, wenn einer der
  Parameter `var`/`out` ist (`Swap(x, x)`).
* Aus einem Feld kann man nicht einfach herausverschieben (`n := h.Next`) – dafür gibt es `Take`.
* `var`-Parameter dürfen beim Verlassen der Routine nicht freigegeben/verschoben zurückbleiben.

**Warnungen** (Übersetzung läuft weiter): Speicherlecks, z. B. ein Besitzer, der am Ende
seines Gültigkeitsbereichs noch ein Objekt besitzt, ein überschriebener Besitzer oder ein
verworfenes `TFoo.Create`.

Die Analyse ist flusssensitiv (if/case/Schleifen mit Fixpunkt-Iteration, `Break`/`Continue`/
`Exit`), erkennt `is some`, `x <> none`, `HasValue` und berücksichtigt, welche nicht-lokalen
Variablen aufgerufene Routinen (transitiv) verändern.
Beispiel: [tests/run/ownership.pas](tests/run/ownership.pas), Fehlerbeispiele in
[tests/borrow_fail/](tests/borrow_fail/).

Grenzen der Analyse: Sie arbeitet pro Routine. Felder und Dereferenzierungen werden nicht
einzeln verfolgt (nur ihre Pfade, um Ausleihen ungültig zu machen); die Lebensdauer von
`borrowed`-Feldern wird nicht geprüft; globale Besitzer werden nur im Hauptprogramm verfolgt;
das Kopieren von Records, die Besitzer enthalten, wird nicht als Move behandelt.

## Optionale Werte: `optional T`

Es gibt kein `nil`. Ein Wert, der fehlen kann, hat den Typ **`optional T`** – für **jeden**
Typ, wie `Option<T>` in Rust und `std::optional<T>` in C++. Der leere Wert heißt **`none`**.
Ein `optional`-Wert kann nicht direkt benutzt werden, er muss erst **ausgepackt** werden:

```pascal
type
  TKnoten = class
    Wert: Integer;
    Next: optional TKnoten;              { Option<Box<Knoten>> }
  end;

function Suche(n: Integer): optional Integer;
begin
  if n > 0 then Result := n * 2          { Integer wird automatisch zu some(...) }
  else Result := none;
end;

val
  alter: optional Integer;               { std::optional<int> }
  p: borrowed optional TKnoten;          { Option<&Knoten> }
var
  kopf: optional TKnoten;
begin
  alter := 42;
  if alter is some a then WriteLn(a);              { if let Some(a) = alter }
  p := kopf;
  while p is some k do begin                       { while let Some(k) = p }
    Write(k.Wert, ' ');
    p := k.Next;
  end;
  if (p is some k) and (k.Wert > 10) then …        { Bindung + weitere Bedingung }
  case Suche(5) of                                  { match }
    some v: WriteLn('gefunden ', v);
    none:   WriteLn('nichts');
  end;
  WriteLn(alter.ValueOr(0));                       { unwrap_or / value_or }
  if alter.HasValue then …                         { is_some / has_value }
  if alter = none then …                           { is_none }
end.
```

Regeln:

* **Auspacken:** `x is some v` (in `if`- und `while`-Bedingungen, auch mit `and`
  verknüpft), `case x of some v: … none: … end`, `x.ValueOr(d)` (für Werte),
  `x.HasValue`, `x = none` / `x <> none`. Die Variable `v` gilt im `then`-Zweig bzw. im
  Schleifenrumpf. Bei Objekten ist `v` eine Ausleihe; bei `Take(x) is some v` übernimmt `v`
  den Besitz.
* `x.Feld` oder `x.Methode` auf einem `optional`-Wert ist ein Fehler – mit dem Hinweis,
  wie man auspackt. Ausnahme: `x.Free` (gibt nichts frei, wenn `none`).
* Ein Wert vom Typ `T` wird automatisch zu `optional T`; umgekehrt nie.
* `none` passt nur zu `optional`-Typen; `x = none` bei einem nicht-optionalen `x` ist ein Fehler.
* Optionale Objekte sind intern ein Zeiger (`none` = NULL) und kosten nichts zusätzlich;
  optionale Werte sind ein kleines Struct aus Flag und Wert.

Für **nicht-optionale** Objekte garantiert der Compiler, dass sie zur Laufzeit immer ein
Objekt haben:

* **Nicht allozierte Heapobjekte:** `var a: TFoo; … a.V := 1;` ohne vorheriges `TFoo.Create`
  ist ein Fehler – auch wenn die Allokation nur auf einem Teil der Pfade passiert.
  Dasselbe gilt für Stackobjekte ohne `x.Create(...)`.
* **Konstruktoren** müssen allen eigenen nicht-optionalen Feldern auf jedem Pfad einen Wert
  geben, bevor `Self` benutzt wird (Methodenaufruf, Übergabe, Speichern). Erbt eine Klasse
  solche Felder, muss der Konstruktor einen geerbten Konstruktor aufrufen. Eine Klasse mit
  nicht-optionalen Feldern muss einen eigenen Konstruktor haben und darf nicht mit einem
  geerbten Konstruktor erzeugt werden, der ihre Felder nicht kennt.
* **Speicher, der leer beginnt** (Array-Elemente, Record-Felder vom Klassentyp) muss
  `optional` sein.
* **Globale Variablen und äußere lokale Variablen:** Ein Aufruf einer Routine, die eine noch
  nicht belegte Variable liest, ist ein Fehler (transitiv über Aufrufe).
* `out`-Parameter müssen belegt werden; Funktionen mit nicht-optionalem Ergebnis müssen ein
  Objekt liefern.
* `Take()` und `FreeAndNil()` sind nur auf `optional`-Plätzen erlaubt.

Weil diese Garantie beim Übersetzen entsteht, erzeugt ppc **keine** Prüfungen im
Programm. Mit `--no-nil-check` wird die Allokationsanalyse abgeschaltet und stattdessen jeder
Zugriff über eine Referenz zur Laufzeit geprüft:

```
Runtime error 210: nil pointer dereference at programm.pas(42,7)
```

Ein fehlgeschlagener Speicherwunsch (`Create`) endet mit `Runtime error 203: heap overflow`.

## Unterstützte Sprache

* `program`, `uses` (ignoriert), `const` (auch typisierte Konstanten und Array-Initialisierer),
  `type`, `val` (Werte und Stackobjekte, auch `val x: Integer = 5;`), `var` (Heapobjekte und Heapwerte)
* Typen: `Integer`/`LongInt` (32 Bit), `ShortInt`, `SmallInt`, `Byte`, `Word`, `Cardinal`,
  `Int64`, `Boolean`, `Char`, `Real`/`Double`, `String` (ShortString, max. 255 Zeichen),
  Aufzählungen, Teilbereiche, statische (auch mehrdimensionale) Arrays, Records (mit Methoden),
  `class` (Stack- oder Heapobjekte)
* Prozeduren/Funktionen, beliebig verschachtelt (Zugriff auf äußere Variablen über
  Static Links), `var`/`const`/`out`-Parameter, `forward`, Rekursion, `Result`/Funktionsname,
  `Exit(Wert)`
* Anweisungen: `:=`, `if`, `case` (auch mit Strings und Bereichen), `while`, `repeat`, `for … to/downto`,
  `with`, `Break`, `Continue`, `Exit`
* Ausdrücke: Arithmetik, `div`, `mod`, `shl`, `shr`, `and`/`or`/`xor`/`not` (Kurzschluss bei Boolean),
  Vergleiche, Stringverkettung, `in [a, b..c]`, Typumwandlungen `Integer(x)`, `TFoo(obj)`
* Standardroutinen: `Write(Ln)` mit `:Breite:Nachkommastellen`, `Read(Ln)`, `Length`, `Copy`,
  `Pos`, `Delete`, `Insert`, `UpCase`, `UpperCase`, `LowerCase`, `Concat`, `IntToStr`, `StrToInt`,
  `FloatToStr`, `Str`, `SetLength`, `Ord`, `Chr`, `Succ`, `Pred`, `Inc`, `Dec`, `Odd`, `Abs`, `Sqr`,
  `Sqrt`, `Sin`, `Cos`, `ArcTan`, `Exp`, `Ln`, `Trunc`, `Round`, `Frac`, `Int`, `High`, `Low`,
  `SizeOf`, `Random`, `Randomize`, `Halt`, `Assigned`, `FreeAndNil`, `Take`;
  bei `optional`: `HasValue`, `ValueOr`

Nicht unterstützt: `nil` (ersetzt durch `optional`/`none`), Zeiger (`^`, `@`, `New`, `Dispose`,
`Pointer` – ersetzt durch Heapobjekte von Klassen und `var`-Parameter), `object`-Typen (ersetzt durch Stackobjekte von Klassen), Units, Mengentypen (außer Mengen-Literalen bei `in`), dynamische/offene
Arrays, Exceptions (`try`/`raise`), Interfaces, Überladen, Klassenmethoden, Prozedurtypen,
varianten Records, `goto`, Datei-I/O.

## Aufbau

| Datei             | Inhalt                                                     |
|-------------------|------------------------------------------------------------|
| `src/parser.c`    | Lexer (case-sensitiv) und rekursiver Abstiegsparser        |
| `src/sema.c`      | Namensauflösung, Typprüfung, Klassenlayout, VMT-Slots      |
| `src/borrow.c`    | Borrow Checker und Allokations-/Initialisierungsanalyse    |
| `src/codegen.c`   | C-Codeerzeugung (Structs, VMTs, Static Links, Runtime-Aufrufe) |
| `src/ppc_rt.h`    | C-Laufzeitbibliothek, wird in jedes Programm eingebettet   |
| `src/main.c`      | Treiber, Prelude (`TObject`), Aufruf des C-Compilers       |
| `tests/`          | Testprogramme und `run.sh`                                 |

Klassen werden als C-Structs mit `__vmt`-Zeiger am Anfang abgebildet. Geerbte Felder liegen
als Präfix vorne, daher sind Zeiger auf Nachfahren mit Zeigern auf Vorfahren layoutkompatibel.
Ein Heapobjekt ist ein mit `calloc` angelegtes Struct; ein Stackobjekt ist dasselbe Struct
als lokale C-Variable, die Pascal-Variable zeigt darauf. Am Funktionsende ruft der erzeugte
Code für jedes konstruierte Stackobjekt den virtuellen Destruktor ohne Freigabe auf.
