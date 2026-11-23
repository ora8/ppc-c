# ppc – Perfect Pascal Compiler

`ppc` (**Perfect Pascal Compiler**) übersetzt einen objektorientierten Pascal-Dialekt (Turbo Pascal / Delphi / Free Pascal)
nach **C** und ruft danach `gcc` auf, um ein natives Programm zu erzeugen.
Der Compiler ist selbst in C geschrieben (C11, keine Abhängigkeiten).

Besonderheiten:

* **Groß-/Kleinschreibung wird unterschieden** (anders als Standard-Pascal).
* **Stack- und Heapobjekte derselben Klasse:** Eine Klassenvariable im Abschnitt `var` ist
  ein **Stackobjekt** (automatisches `Destroy` am Blockende), im Abschnitt `heap` ein
  **Heapobjekt** (`Create`/`Free` von Hand).
* **Borrow Checker** nach dem Vorbild von Rust: prüft Besitz (Ownership) und Ausleihen (Borrows)
  zur Compile-Zeit.
* **Keine unerwarteten nil-Pointer:** Referenzen sind nie nil, außer ihr Typ ist ausdrücklich
  `nilable` – dann erzwingt der Compiler eine Prüfung vor jeder Benutzung. Nicht allozierte
  Heapobjekte, nicht konstruierte Stackobjekte und Konstruktoren, die Felder vergessen,
  werden beim Übersetzen erkannt.

## Bauen und benutzen

```bash
make            # erzeugt ./ppc
make test       # Testsuite (Ausgaben, Borrow-/Nil-/Stackobjekt-Fehler, Compile-Fehler)
```

```bash
./ppc programm.pas            # erzeugt ./programm
./ppc -o hallo programm.pas   # Name des Programms
./ppc -S programm.pas         # nur C-Code erzeugen (programm.c)
./ppc -k programm.pas         # C-Datei zusätzlich behalten
./ppc --no-borrow-check x.pas # Borrow Checker abschalten
./ppc --no-nil-check x.pas    # Nil-Analyse aus, stattdessen Nil-Prüfung zur Laufzeit
./ppc --cc clang -O3 x.pas    # anderer C-Compiler / Optimierung
```

## Groß-/Kleinschreibung

* Schlüsselwörter werden **klein** geschrieben: `program begin end if then class var heap …`
  (`Begin` ist ein Bezeichner, kein Schlüsselwort).
* Bezeichner sind case-sensitiv: `Count` und `count` sind zwei verschiedene Namen.
* Vordefinierte Namen in Delphi-Schreibweise: `Integer`, `Boolean`, `Char`, `Real`, `String`,
  `Byte`, `Word`, `Int64`, `Cardinal`, `Pointer`, `True`, `False`, `MaxInt`, `Pi`,
  `WriteLn`, `Write`, `ReadLn`, `Read`, `Length`, `Ord`, `Chr`, `Inc`, `Dec`, `New`, `Dispose`,
  `TObject`, `Create`, `Destroy`, `Free`, `ClassName`, `Self`, `Result`, `Exit`, `Break`, `Continue` usw.
* Direktiven/Modifizierer klein: `virtual`, `override`, `abstract`, `forward`, `private`,
  `public`, `property … read … write …`, `borrowed`, `owned`, `nilable`.

## Klassen

Referenztypen, implizit von `TObject` abgeleitet:
`constructor Create`, `destructor Destroy; override;`, `Free`, `ClassName`,
`virtual`/`override`/`abstract`, `inherited`, Properties mit Feld- oder Methoden-Zugriff,
`is`/`as`, `Self`, Sichtbarkeitsabschnitte (werden geparst, nicht erzwungen), Forward-Klassen
(`TFoo = class;`).

## Stack- und Heapobjekte

Es gibt **nur eine Art von Klasse**. Wo ein Objekt liegt, entscheidet der Abschnitt, in dem
die Variable deklariert ist:

| Deklaration | Bedeutung |
|---|---|
| `var k: TKreis;` | **Stackobjekt**: das Objekt liegt im Stackframe (global: im Datensegment) |
| `heap h: TKreis;` | **Heapobjekt**: `h` besitzt ein Objekt auf dem Heap |
| `heap n: nilable TKreis;` | Heapobjekt, das auch fehlen darf (nil) |
| `var r: borrowed TKreis;` | Ausleihe: Referenz ohne Besitz auf ein Stack- oder Heapobjekt |

```pascal
procedure Zeige(F: TFigur);         { nimmt Stack- und Heapobjekte }

procedure Demo;
var
  k: TKreis;                        { Stackobjekt }
heap
  h: TFigur;                        { Heapobjekt }
begin
  k.Create(1, 2, 3.0);              { Stack: nur Konstruktor, kein malloc }
  h := TKreis.Create(4, 5, 1.0);    { Heap: Speicher holen + Konstruktor }
  Zeige(k);
  Zeige(h);                         { gleicher Typ, virtuelle Methoden, Polymorphie }
  h.Free;                           { Heap: von Hand freigeben }
end;                                { Stack: k.Destroy wird automatisch aufgerufen }
```

* Klasse, Methoden, Vererbung und VMT sind identisch – nur der Speicherort unterscheidet sich.
* **Stackobjekte** werden mit `k.Create(...)` konstruiert. Der Compiler verlangt das vor der
  ersten Benutzung (auf jedem Pfad) und genau einmal. Am Blockende wird `Destroy`
  automatisch aufgerufen (umgekehrte Deklarationsreihenfolge, globale Stackobjekte am
  Programmende). `k.Free`, `k := …` und Referenzen auf `k`, die den Block verlassen
  (globale Variable, Feld, Funktionsergebnis), sind Fehler.
* **Heapobjekte** werden **nicht automatisch** freigegeben. Der Borrow Checker verfolgt den
  Besitz und warnt bei Speicherlecks; Benutzung nach `Free`, doppeltes `Free` usw. sind Fehler.
* Felder, Parameter und Funktionsergebnisse vom Klassentyp sind Referenzen (siehe unten).
* Für reine Werte mit Kopiersemantik gibt es `record`.

## Borrow Checker

Für Werte von **Klassen- und typisierten Zeigertypen** gilt:

| Konstrukt                      | Bedeutung                                         |
|--------------------------------|---------------------------------------------------|
| `heap a: TFoo;`                | `a` **besitzt** ein Heapobjekt (Owner)            |
| `var r: borrowed TFoo;`        | `r` ist eine **Ausleihe** (besitzt nichts)        |
| `var s: TFoo;`                 | Stackobjekt (gehört dem Block)                    |
| `Next: nilable TItem;` (Feld)  | Feld besitzt das Objekt (darf nil sein)           |
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
* Referenzen auf Stackvariablen und Stackobjekte (`@x`, `var s: TFoo`) dürfen nicht in globale
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
`Exit`), erkennt `if x <> nil` / `Assigned(x)` und berücksichtigt, welche nicht-lokalen
Variablen aufgerufene Routinen (transitiv) verändern.
Beispiel: [tests/run/ownership.pas](tests/run/ownership.pas), Fehlerbeispiele in
[tests/borrow_fail/](tests/borrow_fail/).

Grenzen der Analyse: Sie arbeitet pro Routine. Felder und Dereferenzierungen werden nicht
einzeln verfolgt (nur ihre Pfade, um Ausleihen ungültig zu machen); die Lebensdauer von
`borrowed`-Feldern wird nicht geprüft; globale Besitzer werden nur im Hauptprogramm verfolgt;
das Kopieren von Records, die Besitzer enthalten, wird nicht als Move behandelt.

## Nil-Sicherheit

Klassen- und typisierte Zeigertypen sind **nie nil**. Nur Typen mit dem Zusatz **`nilable`**
dürfen `nil` enthalten – das ist der einzige Weg, „kein Objekt“ auszudrücken (z. B. das Ende
einer Liste oder eine erfolglose Suche):

```pascal
heap a: TFoo;                   { bekommt vor der Benutzung ein Objekt }
     p: nilable TFoo;           { darf nil sein, muss vor der Benutzung geprüft werden }
var  r: borrowed nilable TFoo;  { Reihenfolge: borrowed vor nilable }
function Suche(…): borrowed nilable TPerson;
Next: nilable TPerson;          { Feld, z. B. Listenende }
```

Der Compiler stellt sicher, dass eine nicht-nilable Referenz zur Laufzeit nie nil ist:

* **Nicht allozierte Heapobjekte:** `heap a: TFoo; … a.V := 1;` ohne vorheriges `TFoo.Create`
  bzw. `New(p)` ist ein Fehler – auch wenn die Allokation nur auf einem Teil der Pfade passiert.
  Dasselbe gilt für Stackobjekte ohne `x.Create(...)`.
* **Konstruktoren** müssen allen eigenen nicht-nilable Feldern auf jedem Pfad einen Wert
  geben, bevor `Self` benutzt wird (Methodenaufruf, Übergabe, Speichern). Erbt eine Klasse
  solche Felder, muss der Konstruktor einen geerbten Konstruktor aufrufen. Eine Klasse mit
  nicht-nilable Feldern muss einen eigenen Konstruktor haben und darf nicht mit einem
  geerbten Konstruktor erzeugt werden, der ihre Felder nicht kennt.
* **Speicher, der leer beginnt** (Array-Elemente, Record-Felder, Ziel von `^T`) darf nur
  `nilable`-Referenzen enthalten.
* **Globale Variablen und äußere lokale Variablen:** Ein Aufruf einer Routine, die eine noch
  nicht belegte nicht-nilable Variable liest, ist ein Fehler (transitiv über Aufrufe).
* `out`-Parameter müssen belegt werden; `nil` (oder ein evtl. nil-Wert) darf nicht in eine
  nicht-nilable Variable, ein Feld, einen Parameter oder das Funktionsergebnis gelangen.
* `nilable`-Werte müssen vor der Benutzung geprüft werden: `if p <> nil then`,
  `while p <> nil do`, `Assigned(p)`, `else`-Zweige. `nilable`-Felder und -Ergebnisse
  (`x.Next.V`) werden erst in eine lokale Variable kopiert und dort geprüft (wie in Kotlin).
* `Take()` und `FreeAndNil()` sind nur auf `nilable`-Plätzen erlaubt.

Weil diese Garantie beim Übersetzen entsteht, erzeugt ppc **keine** Nil-Prüfungen im
Programm – es gibt keine Laufzeitkosten. Mit `--no-nil-check` wird die Analyse abgeschaltet
und stattdessen jeder Zugriff über eine Referenz zur Laufzeit geprüft:

```
Runtime error 210: nil pointer dereference at programm.pas(42,7)
```

Ein fehlgeschlagener Speicherwunsch (`Create`/`New`) endet mit `Runtime error 203: heap overflow`.

## Unterstützte Sprache

* `program`, `uses` (ignoriert), `const` (auch typisierte Konstanten und Array-Initialisierer),
  `type`, `var` (auch `var x: Integer = 5;`), `heap`
* Typen: `Integer`/`LongInt` (32 Bit), `ShortInt`, `SmallInt`, `Byte`, `Word`, `Cardinal`,
  `Int64`, `Boolean`, `Char`, `Real`/`Double`, `String` (ShortString, max. 255 Zeichen),
  Aufzählungen, Teilbereiche, statische (auch mehrdimensionale) Arrays, Records, Zeiger,
  `class` (Stack- oder Heapobjekte)
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

Nicht unterstützt: `object`-Typen (ersetzt durch Stackobjekte von Klassen), Units, Mengentypen (außer Mengen-Literalen bei `in`), dynamische/offene
Arrays, Exceptions (`try`/`raise`), Interfaces, Überladen, Klassenmethoden, Prozedurtypen,
varianten Records, `goto`, Datei-I/O.

## Aufbau

| Datei             | Inhalt                                                     |
|-------------------|------------------------------------------------------------|
| `src/parser.c`    | Lexer (case-sensitiv) und rekursiver Abstiegsparser        |
| `src/sema.c`      | Namensauflösung, Typprüfung, Klassenlayout, VMT-Slots      |
| `src/borrow.c`    | Borrow Checker und Nil-Analyse                             |
| `src/codegen.c`   | C-Codeerzeugung (Structs, VMTs, Static Links, Runtime-Aufrufe) |
| `src/ppc_rt.h`    | C-Laufzeitbibliothek, wird in jedes Programm eingebettet   |
| `src/main.c`      | Treiber, Prelude (`TObject`), Aufruf des C-Compilers       |
| `tests/`          | Testprogramme und `run.sh`                                 |

Klassen werden als C-Structs mit `__vmt`-Zeiger am Anfang abgebildet. Geerbte Felder liegen
als Präfix vorne, daher sind Zeiger auf Nachfahren mit Zeigern auf Vorfahren layoutkompatibel.
Ein Heapobjekt ist ein mit `calloc` angelegtes Struct; ein Stackobjekt ist dasselbe Struct
als lokale C-Variable, die Pascal-Variable zeigt darauf. Am Funktionsende ruft der erzeugte
Code für jedes konstruierte Stackobjekt den virtuellen Destruktor ohne Freigabe auf.
