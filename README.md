# ppc – Perfect Pascal Compiler

`ppc` (**Perfect Pascal Compiler**) übersetzt einen objektorientierten Pascal-Dialekt (Turbo Pascal / Delphi / Free Pascal)
nach **C** und ruft danach `gcc` auf, um ein natives Programm zu erzeugen.
Der Compiler ist selbst in C geschrieben (C11, keine Abhängigkeiten).

Besonderheiten:

* **Groß-/Kleinschreibung wird unterschieden** (anders als Standard-Pascal).
* **Stackobjekte** (`object`, Turbo-Pascal-Stil) und **Heap-Klassen** (`class`, Delphi-Stil).
* **Borrow Checker** nach dem Vorbild von Rust: prüft Besitz (Ownership) und Ausleihen (Borrows)
  zur Compile-Zeit.

## Bauen und benutzen

```bash
make            # erzeugt ./ppc
make test       # Testsuite (Ausgaben, Borrow-Fehler, Compile-Fehler)
```

```bash
./ppc programm.pas            # erzeugt ./programm
./ppc -o hallo programm.pas   # Name des Programms
./ppc -S programm.pas         # nur C-Code erzeugen (programm.c)
./ppc -k programm.pas         # C-Datei zusätzlich behalten
./ppc --no-borrow-check x.pas # Borrow Checker abschalten
./ppc --cc clang -O3 x.pas    # anderer C-Compiler / Optimierung
```

## Groß-/Kleinschreibung

* Schlüsselwörter werden **klein** geschrieben: `program begin end if then class object …`
  (`Begin` ist ein Bezeichner, kein Schlüsselwort).
* Bezeichner sind case-sensitiv: `Count` und `count` sind zwei verschiedene Namen.
* Vordefinierte Namen in Delphi-Schreibweise: `Integer`, `Boolean`, `Char`, `Real`, `String`,
  `Byte`, `Word`, `Int64`, `Cardinal`, `Pointer`, `True`, `False`, `MaxInt`, `Pi`,
  `WriteLn`, `Write`, `ReadLn`, `Read`, `Length`, `Ord`, `Chr`, `Inc`, `Dec`, `New`, `Dispose`,
  `TObject`, `Create`, `Destroy`, `Free`, `ClassName`, `Self`, `Result`, `Exit`, `Break`, `Continue` usw.
* Direktiven/Modifizierer klein: `virtual`, `override`, `abstract`, `forward`, `private`,
  `public`, `property … read … write …`, `borrowed`, `owned`.

## Objekte auf dem Stack (`object`)

```pascal
type
  TShape = object
    X, Y: Integer;
    constructor Init(AX, AY: Integer);
    function Area: Real; virtual;
    destructor Done; virtual;
  end;
  TCircle = object(TShape)
    R: Real;
    constructor Init(AX, AY: Integer; AR: Real);
    function Area: Real; virtual;
  end;

var c: TCircle;          { liegt auf dem Stack – kein New/Free nötig }
begin
  c.Init(1, 2, 1.5);
  WriteLn(c.Area:0:2);
end.
```

* Objekte sind Werttypen: lokale Variablen liegen im Stackframe, globale im Datensegment,
  sie können in Records/Arrays eingebettet, kopiert und als Wert zurückgegeben werden.
* Virtuelle Methoden funktionieren auch ohne Konstruktoraufruf: ppc initialisiert den
  VMT-Zeiger von Objektvariablen automatisch.
* Polymorphie über `var`-Parameter (`procedure Show(var S: TShape)` akzeptiert ein `TCircle`)
  und über Zeiger (`PShape`).
* Heap-Objekte wie in Turbo Pascal: `New(p, Init(…))`, `p := New(PCircle, Init(…))`,
  `Dispose(p, Done)`.
* Zuweisung Nachfahre → Vorfahre (`base := circle`) kopiert die Felder des Vorfahren,
  **der VMT-Zeiger des Ziels bleibt erhalten** (FPC kopiert ihn mit und ruft dann
  Methoden des Nachfahren auf einem zu kleinen Objekt auf – ppc vermeidet das bewusst).

## Klassen (`class`)

Referenztypen auf dem Heap, implizit von `TObject` abgeleitet:
`constructor Create`, `destructor Destroy; override;`, `Free`, `ClassName`,
`virtual`/`override`/`abstract`, `inherited`, Properties mit Feld- oder Methoden-Zugriff,
`is`/`as`, `Self`, Sichtbarkeitsabschnitte (werden geparst, nicht erzwungen), Forward-Klassen
(`TFoo = class;`).

## Borrow Checker

Für Werte von **Klassen- und typisierten Zeigertypen** gilt:

| Konstrukt                      | Bedeutung                                         |
|--------------------------------|---------------------------------------------------|
| `var a: TFoo;`                 | `a` **besitzt** das Objekt (Owner)                |
| `var r: borrowed TFoo;`        | `r` ist eine **Ausleihe** (besitzt nichts)        |
| `Next: TItem;` (Feld)          | Feld besitzt das Objekt                           |
| `Parent: borrowed TItem;`      | Feld ist eine Ausleihe                            |
| `procedure P(x: TFoo)`         | Wert-/`const`-Parameter = Ausleihe                |
| `procedure P(owned x: TFoo)`   | Parameter übernimmt den Besitz                    |
| `procedure P(var x: TFoo)`     | veränderliche Ausleihe des Besitzer-Platzes       |
| `function F: TFoo`             | Ergebnis wird an den Aufrufer übergeben           |
| `function F: borrowed TFoo`    | Ergebnis ist eine Ausleihe (z. B. `Find`)         |
| `Take(x.Next)`                 | holt den Wert heraus und setzt den Platz auf `nil`|

Regeln (Verstöße sind **Fehler**, die Übersetzung bricht ab):

* `b := a` zwischen zwei Besitzern ist ein **Move**; `a` ist danach unbenutzbar
  (`use of moved value 'a'`), bis es neu zugewiesen wird.
* Kein Zugriff nach `Free`/`Destroy`/`Dispose` (`use after free`), kein doppeltes Freigeben.
* Ausleihen dürfen nicht freigegeben und nicht in Besitzer gespeichert werden.
* Eine Ausleihe merkt sich, woher sie stammt. Wird die Quelle freigegeben, verschoben oder
  neu zugewiesen, ist die Ausleihe **hängend**; ihre Benutzung ist ein Fehler.
* Referenzen auf Stackvariablen (`@x`) dürfen nicht in globale Variablen, Felder oder
  Funktionsergebnisse entkommen; Funktionen dürfen keine Referenz auf Lokales zurückgeben.
* Dieselbe Variable darf in einem Aufruf nicht zweimal übergeben werden, wenn einer der
  Parameter `var`/`out` ist (`Swap(x, x)`).
* Aus einem Feld kann man nicht einfach herausverschieben (`n := h.Next`) – dafür gibt es `Take`.
* `var`-Parameter dürfen beim Verlassen der Routine nicht freigegeben/verschoben zurückbleiben.

**Warnungen** (Übersetzung läuft weiter): Speicherlecks, z. B. ein Besitzer, der am Ende
seines Gültigkeitsbereichs noch ein Objekt besitzt, ein überschriebener Besitzer oder ein
verworfenes `TFoo.Create`.

Die Analyse ist flusssensitiv (if/case/Schleifen mit Fixpunkt-Iteration, `Break`/`Continue`/
`Exit`), erkennt `if x <> nil` / `Assigned(x)` und berücksichtigt, welche nicht-lokalen
Variablen aufgerufene Routinen (transitiv) verändern.
Beispiel: [tests/run/ownership.pas](tests/run/ownership.pas), Fehlerbeispiele in
[tests/borrow_fail/](tests/borrow_fail/).

Grenzen der Analyse: Sie arbeitet pro Routine. Felder und Dereferenzierungen werden nicht
einzeln verfolgt (nur ihre Pfade, um Ausleihen ungültig zu machen); die Lebensdauer von
`borrowed`-Feldern wird nicht geprüft; globale Besitzer werden nur im Hauptprogramm verfolgt;
das Kopieren von Records, die Besitzer enthalten, wird nicht als Move behandelt.

## Unterstützte Sprache

* `program`, `uses` (ignoriert), `const` (auch typisierte Konstanten und Array-Initialisierer),
  `type`, `var` (auch `var x: Integer = 5;`)
* Typen: `Integer`/`LongInt` (32 Bit), `ShortInt`, `SmallInt`, `Byte`, `Word`, `Cardinal`,
  `Int64`, `Boolean`, `Char`, `Real`/`Double`, `String` (ShortString, max. 255 Zeichen),
  Aufzählungen, Teilbereiche, statische (auch mehrdimensionale) Arrays, Records, Zeiger,
  `object`, `class`
* Prozeduren/Funktionen, beliebig verschachtelt (Zugriff auf äußere Variablen über
  Static Links), `var`/`const`/`out`-Parameter, `forward`, Rekursion, `Result`/Funktionsname,
  `Exit(Wert)`
* Anweisungen: `:=`, `if`, `case` (auch mit Strings und Bereichen), `while`, `repeat`, `for … to/downto`,
  `with`, `Break`, `Continue`, `Exit`
* Ausdrücke: Arithmetik, `div`, `mod`, `shl`, `shr`, `and`/`or`/`xor`/`not` (Kurzschluss bei Boolean),
  Vergleiche, Stringverkettung, `in [a, b..c]`, `@`, `^`, Typumwandlungen `Integer(x)`, `TFoo(obj)`
* Standardroutinen: `Write(Ln)` mit `:Breite:Nachkommastellen`, `Read(Ln)`, `Length`, `Copy`,
  `Pos`, `Delete`, `Insert`, `UpCase`, `UpperCase`, `LowerCase`, `Concat`, `IntToStr`, `StrToInt`,
  `FloatToStr`, `Str`, `SetLength`, `Ord`, `Chr`, `Succ`, `Pred`, `Inc`, `Dec`, `Odd`, `Abs`, `Sqr`,
  `Sqrt`, `Sin`, `Cos`, `ArcTan`, `Exp`, `Ln`, `Trunc`, `Round`, `Frac`, `Int`, `High`, `Low`,
  `SizeOf`, `Random`, `Randomize`, `Halt`, `Assigned`, `New`, `Dispose`, `FreeAndNil`, `Take`

Nicht unterstützt: Units, Mengentypen (außer Mengen-Literalen bei `in`), dynamische/offene
Arrays, Exceptions (`try`/`raise`), Interfaces, Überladen, Klassenmethoden, Prozedurtypen,
varianten Records, `goto`, Datei-I/O.

## Aufbau

| Datei             | Inhalt                                                     |
|-------------------|------------------------------------------------------------|
| `src/parser.c`    | Lexer (case-sensitiv) und rekursiver Abstiegsparser        |
| `src/sema.c`      | Namensauflösung, Typprüfung, Objekt-/Klassenlayout, VMT-Slots |
| `src/borrow.c`    | Borrow Checker                                             |
| `src/codegen.c`   | C-Codeerzeugung (Structs, VMTs, Static Links, Runtime-Aufrufe) |
| `src/ppc_rt.h`    | C-Laufzeitbibliothek, wird in jedes Programm eingebettet   |
| `src/main.c`      | Treiber, Prelude (`TObject`), Aufruf des C-Compilers       |
| `tests/`          | Testprogramme und `run.sh`                                 |

Klassen werden als C-Structs mit `__vmt`-Zeiger am Anfang abgebildet, Objekte (`object`) als
Werte-Structs, bei denen der VMT-Zeiger dort eingefügt wird, wo die erste virtuelle Methode
eingeführt wird. Geerbte Felder liegen als Präfix vorne, daher sind Zeiger auf Nachfahren mit
Zeigern auf Vorfahren layoutkompatibel.
