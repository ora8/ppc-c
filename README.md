# ppc – Perfect Pascal Compiler

`ppc` (**Perfect Pascal Compiler**) ist der Compiler für **Perfect Pascal**, eine Weiterentwicklung von
Pascal und Delphi. Die Sprache übernimmt von **Pascal** die klare Syntax (`begin … end`, `:=`,
`procedure`/`function`, Records, Aufzählungen, Teilbereiche, `case`, `for … to/downto`) und von
**Delphi** die Objektorientierung (Klassen mit `constructor`/`destructor`, `virtual`/`override`,
`inherited`, Properties, `is`/`as`, Klassenmethoden, Records mit Methoden, Generics mit `<T>`).
Darüber hinaus entwickelt sie beide weiter – mit Ideen aus Rust: Besitz und Ausleihen statt
manueller Speicherverwaltung, `optional T` statt `nil`, `result T` statt Exceptions,
Stack- und Heapobjekte derselben Klasse und unterschiedenes Groß-/Kleinschreiben.

Perfect Pascal ist daher **nicht** quellcodekompatibel zu Delphi oder Free Pascal: Vertrautes sieht
vertraut aus, aber `var`/`val`/`vao`, die Pflicht zur Initialisierung und die Regeln für Besitz
unterscheiden sich bewusst.

`ppc` übersetzt Perfect Pascal nach **C** und ruft danach `gcc` auf, um ein natives Programm zu
erzeugen. Der Compiler ist selbst in C geschrieben (C11, keine Abhängigkeiten).

Besonderheiten:

* **Groß-/Kleinschreibung wird unterschieden** (anders als Standard-Pascal).
* **Drei Arten von Variablen:** `val` für Werte und Stackobjekte, `var` für Referenzen ohne
  Besitz und `vao` für Besitzer von Heapobjekten. Ein `vao` gibt sein Objekt automatisch frei,
  wenn es den Gültigkeitsbereich verlässt (wie `Box<T>` in Rust); ein Stackobjekt wird am
  Blockende abgebaut. Jede Variable wird bei der Deklaration initialisiert.
* **Borrow Checker** nach dem Vorbild von Rust: prüft Besitz (Ownership), Ausleihen (Borrows)
  und die Ausleihregeln (viele lesende *oder* eine änderbare Ausleihe) zur Compile-Zeit.
* **Keine Zeiger und kein nil:** Es gibt kein `^`, `@`, `New`, `Dispose` und kein `nil`.
  Werte, die fehlen können, haben den Typ **`optional T`** (wie `Option<T>` in Rust bzw.
  `std::optional<T>` in C++) und müssen vor der Benutzung ausgepackt werden.
* **Keine Exceptions:** Fehler sind Rückgabewerte vom Typ **`result T`** (wie `Result<T, E>` in
  Rust), weitergegeben mit `try`.
* **`T const`** macht Variablen, Parameter und Objekt-Referenzen unveränderlich;
  `; const;` kennzeichnet Methoden, die `Self` nicht ändern.
* **Klassenmethoden** (`class function`), **Records mit Methoden**, Record- und Array-Literale.

## Bauen und benutzen

```bash
make            # erzeugt ./ppc
make test       # Testsuite (Ausgaben, Borrow-/Optional-/Stackobjekt-/const-/result-/Clone-Fehler, Compile-Fehler)
```

```bash
./ppc programm.pas            # erzeugt ./programm
./ppc -o hallo programm.pas   # Name des Programms
./ppc -S programm.pas         # nur C-Code erzeugen (programm.c)
./ppc -k programm.pas         # C-Datei zusätzlich behalten
./ppc --no-borrow-check x.pas # Borrow Checker abschalten
./ppc --no-nil-check x.pas    # Allokationsanalyse aus, stattdessen Prüfung zur Laufzeit
./ppc --no-range-check x.pas  # keine Prüfung der Array-/String-Indizes zur Laufzeit
./ppc --cc clang -O3 x.pas    # anderer C-Compiler / Optimierung
```

## Fehlermeldungen

Der Compiler bricht nicht beim ersten Fehler ab, sondern meldet möglichst alle auf einmal:

```
prog.pas(4,15) Error: ')' expected but number 1 found
prog.pas(6,8) Error: expression expected but ':=' found
prog.pas(15,6) Error: expression expected but 'then' found
prog.pas: 3 syntax error(s), compilation aborted
```

* **Syntaxfehler:** Nach einem Fehler überspringt der Parser die kaputte Anweisung bis zum
  nächsten `;` bzw. zum Ende des Blocks (bei einer Deklaration bis zu ihrem Ende, bei einer
  Routine samt Rumpf) und liest danach weiter.
* **Typfehler:** Ist der Text syntaktisch in Ordnung, prüft der Compiler jede Anweisung und jede
  Deklaration für sich; eine fehlerhafte wird übersprungen. Ist die Deklaration einer Variablen
  kaputt, werden ihre späteren Benutzungen nicht noch einmal gemeldet.
* **Borrow Checker:** meldet ebenfalls alle gefundenen Fehler (er läuft nur, wenn es keine
  Syntax- oder Typfehler gibt).
* Nach 20 Fehlern hört der Compiler auf (`too many errors (20), stopping`).

## Groß-/Kleinschreibung

* Schlüsselwörter werden **klein** geschrieben: `program begin end if then class val var vao …`
  (`Begin` ist ein Bezeichner, kein Schlüsselwort).
* Bezeichner sind case-sensitiv: `Count` und `count` sind zwei verschiedene Namen.
* Vordefinierte Namen in Delphi-Schreibweise: `Integer`, `Boolean`, `Char`, `Real`, `String`,
  `Byte`, `Word`, `Int64`, `Cardinal`, `True`, `False`, `MaxInt`, `Pi`,
  `WriteLn`, `Write`, `ReadLn`, `Read`, `Length`, `Ord`, `Chr`, `Inc`, `Dec`,
  `TObject`, `Create`, `Destroy`, `Free`, `ClassName`, `Self`, `Result`, `Exit`, `Break`, `Continue` usw.
* Direktiven/Modifizierer klein: `virtual`, `override`, `abstract`, `forward`, `private`,
  `public`, `property … read … write …`, `optional`, `some`, `none`, `default`, `result`, `ok`, `error`, `try`, `auto`.

## Klassen

Referenztypen, implizit von `TObject` abgeleitet:
`constructor Create`, `destructor Destroy; override;`, `Free`, `ClassName`,
`virtual`/`override`/`abstract`, `inherited`, Properties mit Feld- oder Methoden-Zugriff,
`is`/`as`, `Self`, Sichtbarkeitsabschnitte (werden geparst, nicht erzwungen), Forward-Klassen
(`TFoo = class;`). Objekte werden nur im Initialisierer erzeugt: als **Stackobjekt**
`val k := TKreis.Create(1, 2, 3.0)` oder als **Heapobjekt** `vao k := TKreis.Create(1, 2, 3.0)`.

### Klassenmethoden: `class function` / `class procedure`

Wie in Delphi gehören Klassenmethoden zur Klasse, nicht zu einem Objekt. Sie haben kein
`Self` und werden über den Typnamen aufgerufen (auch bei Records):

```pascal
type
  TKonto = class
    Saldo: Integer;
    class function Eroeffne(AName: String const): TKonto;
  end;
  TPunkt = record
    X, Y: Integer;
    class function Ursprung: TPunkt;
  end;

class function TKonto.Eroeffne(AName: String const): TKonto;
begin Result := TKonto.Create(AName, 10); end;
class function TPunkt.Ursprung: TPunkt; begin Result := (X: 0; Y: 0); end;
…
vao k := TKonto.Eroeffne('Anna');
val p := TPunkt.Ursprung;
```

* Im Rumpf sind andere Klassenmethoden ohne Präfix aufrufbar; Felder, normale Methoden,
  `Self` und `inherited` gibt es dort nicht.
* Klassenmethoden können nicht `virtual`, `abstract`, `override` oder `; const;` sein;
  Konstruktoren und Destruktoren sind keine Klassenmethoden.
* Die Implementierung wiederholt `class` (`class function TKonto.Eroeffne …`).

## Variablen: `val`, `var` und `vao`

Neben `const` und `type` gibt es drei Variablenabschnitte. **Jede Variable wird bei der
Deklaration mit `:=` initialisiert**; der Typ darf fehlen, dann kommt er aus dem Initialisierer.

| | was die Variable enthält | besitzt? | gibt frei? | Rust | C++ |
|---|---|---|---|---|---|
| **`val`** | den **Wert selbst** (Zahl, String, Record, Array, Stackobjekt) | ja | am Blockende (Stackobjekt: `Destroy`) | `let x: T` | `T x` |
| **`vao`** | eine **besitzende Referenz** auf ein Objekt oder einen Wert auf dem Heap | ja | am Ende des Bereichs | `Box<T>` | `std::unique_ptr<T>` |
| **`var`** | eine **gewöhnliche Referenz** auf etwas, das jemand anderem gehört | nein | nie | `&mut T` (`T const`: `&T`) | `T&` (`T const`: `const T&`) |

Für Parameter gilt dasselbe Schema: `vao x: TFoo` übernimmt den Besitz, `var x: T` ist eine
änderbare Referenz, `x: T` leiht ein Objekt aus bzw. kopiert einen einfachen Wert.

| Deklaration | Bedeutung |
|---|---|
| `val i := 5;` / `val i: Integer := 5;` | **Wert** (Zahlen, Strings, Records, Arrays, `optional`-Werte) |
| `val v: TVektor := default;` | Nullwert des Typs (0, `''`, `False`, `none`, alle Felder 0) |
| `val s := TKreis.Create(1, 2, 3.0);` | **Stackobjekt**: liegt im Stackframe, `Destroy` am Blockende, siehe unten |
| `vao k := TKreis.Create(1, 2, 3.0);` | **Besitzer**: `k` besitzt das Objekt und gibt es am Ende frei |
| `vao n: optional TKreis := none;` | Besitzer, der (noch) nichts besitzen darf |
| `vao I: Integer := 42;` | **Heapwert**: ein Wert auf dem Heap, siehe unten |
| `var r: TKreis := k;` | **Referenz ohne Besitz** (Ausleihe) auf ein Objekt; gibt nichts frei |
| `var x: Integer := i;` | **Referenz auf eine Variable**: `x` ist ein zweiter Name für `i` (wie `int&` in C++) |

```pascal
procedure Demo;
vao
  k := TKreis.Create(4, 5, 1.0);      { besitzt das Objekt }
var
  r: TFigur := k;                     { leiht es aus }
val
  n := 3;
begin
  Zeige(r);                           { gleicher Typ, virtuelle Methoden, Polymorphie }
end;                                  { k wird hier freigegeben, r nicht }
```

* Eine Klassenvariable in `val` ist ein Stackobjekt und muss mit einem Konstruktor erzeugt werden;
  `optional` geht nur mit `vao` oder `var`.
* Die Initialisierer dürfen beliebige Ausdrücke sein. Sie werden ausgewertet, wenn die
  Deklaration ausgeführt wird: beim Eintritt in die Routine bzw. in den Block (siehe „Blöcke“).
* **Besitzer (`vao`)** geben ihr Objekt **automatisch frei**, wenn sie den Gültigkeitsbereich
  verlassen (Routine, Block, Programm) – außer der Besitz wurde vorher weitergegeben: als
  Funktionsergebnis (`Result := h`), über einen `var`-Parameter, an einen `vao`-Parameter, in ein
  Feld oder einen anderen Besitzer, per `Take`. Wird ein Besitzer überschrieben, wird sein altes
  Objekt freigegeben; gibt ein Objekt frei, werden seine besitzenden Felder mit freigegeben;
  temporäre Objekte, die niemand übernimmt, am Ende der Anweisung.
* `h.Free` ist erlaubt, um **früher** freizugeben. Benutzung danach und doppeltes `Free` sind
  Fehler.
* **Referenzen (`var`)** besitzen nichts. Bei Objekten sind sie Ausleihen; bei Werten
  verweisen sie auf eine andere Variable (oder ein Feld, Array-Element, einen Heapwert) –
  Lesen und Schreiben über `x` wirkt direkt auf das Original. Der Initialisierer muss eine
  Variable sein, kein berechneter Wert. Für beide gelten die Ausleihregeln (siehe
  „Borrow Checker“).
* `var` bei Parametern (`procedure P(var x: Integer)`) bedeutet wie immer Übergabe per Referenz.

### Stackobjekte: `val k := TKlasse.Create(...)`

Wenn Geschwindigkeit zählt, kann ein Objekt direkt im Stackframe liegen (global: im
Datensegment) – ohne `malloc` und `free`. Klasse, Methoden, Vererbung und VMT sind dieselben wie
bei Heapobjekten:

```pascal
procedure Male(F: TFigur);                  { nimmt Stack- und Heapobjekte }

procedure Lokal;
val k := TKreis.Create(1, 2.5);             { im Stackframe konstruiert }
begin
  Male(k);                                  { Ausleihe, virtuelle Methoden wie gewohnt }
  k.Name := TFigur.Create(99);              { besitzende Felder gehen auch }
  var f: TFigur := k;                       { Referenz auf das Stackobjekt }
  begin f.X := 5; end;
end;                                        { k.Destroy läuft hier, k.Name wird freigegeben }
```

* Das Objekt wird bei der Deklaration konstruiert und am Ende seines Blocks automatisch abgebaut
  (`Destroy`, danach werden seine besitzenden Felder freigegeben) – auch bei `Break`, `Exit` und
  `try`. In einem Schleifenrumpf entsteht in jedem Durchlauf ein frisches Objekt.
* **Verschieben** in ein anderes Stackobjekt geht wie in Rust: `val b := k` kopiert das Objekt
  byteweise nach `b` (ohne Konstruktor, ohne `malloc`); `k` ist danach tot – seine Benutzung und
  Ausleihen von `k` sind Fehler (`use of moved stack object 'k'`), und am Blockende wird nur `b`
  abgebaut. Geschieht der Move nur auf einem Pfad, merkt sich das Programm, ob `k` noch abgebaut
  werden muss.
* **Auf den Heap verschieben** (wie `Box::new` in Rust): Bekommt ein Besitzer das Stackobjekt –
  `vao h := k`, `Result := k`, `obj.Feld := k`, ein `vao`-Parameter –, holt das Programm einmal
  Heap-Speicher und kopiert das Objekt byteweise hinein (besitzende Felder wandern mit, nichts
  wird geklont). `k` ist danach tot wie bei jedem Move. So lässt sich ein Objekt schnell auf dem
  Stack aufbauen und erst auf den Heap geben, wenn es den Block überleben soll:

  ```pascal
  function Baue(n: Integer): TK;
  val k := TK.Create(n);
  begin
    k.Teil := TK.Create(n + 100);
    Result := k;                    { Objekt samt Teil wandert auf den Heap }
  end;
  ```
* **Vom Heap auf den Stack:** `val k := h` (oder ein Funktionsergebnis, `val k := Mach(1)`)
  kopiert das Objekt in den Stackframe und gibt den Heap-Speicher zurück; `h` ist danach leer.
  Die Klasse muss genau passen – ist das Objekt zur Laufzeit eine Unterklasse, stoppt das
  Programm mit `Runtime error 219: a TBar object does not fit into stack object 's' (TFoo)`.
* Zusammengefasst:

  | von → nach | `val` (Stack) | `vao` (Heap) | `var` (Referenz) |
  |---|---|---|---|
  | `val k` | Move (`memcpy`) | Move auf den Heap | Ausleihe |
  | `vao h` | Move auf den Stack | Move (Zeiger) | Ausleihe |
  | `var r` | nur als Kopie: `val k := r.Clone` | nur als Kopie: `vao h := r.Clone` | Ausleihe |

  Aus einer Referenz kann man nicht verschieben, weil das Objekt weiter seinem Besitzer gehört.
* Ein Stackobjekt kann nicht neu zugewiesen und nicht freigegeben werden. Eine Kopie, die das
  Original behält: `vao t := k.Clone`.
* Ausleihen (`var r: TFigur := k`, Parameter) sind erlaubt, dürfen aber den Block nicht
  überleben: Speichern in globalen Variablen, Feldern oder Funktionsergebnissen meldet der
  Borrow Checker.
* Deklarierter Typ und Konstruktor-Klasse müssen gleich sein (`val f: TFigur := TKreis.Create`
  ist ein Fehler, weil das Objekt in seiner genauen Größe angelegt wird).
* Messung: 50 Millionen kurzlebige Objekte brauchen mit `-O2` 0,08 s als Stackobjekt und 0,32 s
  als Heapobjekt.

### Dynamische Arrays: `array of T`

Ein dynamisches Array besitzt einen Puffer auf dem Heap, der mit `SetLength` wächst und schrumpft
(Index ab 0, wie in Delphi):

```pascal
val zahlen: array of Integer := (1, 2, 3);
vao kunden: array of optional TKunde := default;   { leer }
begin
  SetLength(zahlen, 5);                     { neue Elemente sind 0 }
  zahlen[4] := 42;
  WriteLn(Length(zahlen), ' ', High(zahlen));   { 5 4 }
  SetLength(kunden, 1);
  kunden[0] := TKunde.Create('Anna');
end;                                        { Puffer und Kunden werden freigegeben }

function Summe(a: array of Integer): Integer;     { Ausleihe, keine Kopie }
procedure Fuelle(var a: array of Integer; n: Integer);   { darf die Länge ändern }
```

* `Length(a)`, `High(a)` (= `Length(a) - 1`), `Low(a)` (= 0); jeder Zugriff wird geprüft
  (`Runtime error 201: … index 3 is outside 0..2`).
* `SetLength` gibt abgeschnittene Elemente frei (wenn sie Objekte besitzen); neue beginnen mit dem
  Nullwert bzw. `none`. Weil der Puffer dabei umziehen kann, sind Ausleihen von Elementen danach
  ungültig – der Borrow Checker meldet ihre Benutzung
  (`… used after its owner 'a' was resized by SetLength at line 3`).
* Das Array besitzt seinen Puffer: `b := a` **verschiebt** (`a` ist danach leer), `a.Clone` kopiert
  (samt Klonen enthaltener Objekte); als Parameter wird es ausgeliehen.
* Funktioniert auch generisch, z. B. ein `TStapel<T>` mit `Elemente: array of optional T`.

### Arrays und Records, die Objekte besitzen

Ein Array oder Record mit besitzenden Elementen bzw. Feldern (`array[1..9] of optional TK`,
`record L: optional TK; end`) besitzt diese Objekte – auch in einer `val`-Variablen:

```pascal
val
  a: array[1..2] of optional TK := default;
  q: TPaar := default;
  z: TPaar := default;
begin
  a[1] := TK.Create(1);        { das Objekt gehört jetzt dem Array }
  q := Mach(4);
  z := q;                      { Move wie in Rust: q ist danach leer (alle Felder none) }
  q := Mach(5);                { altes q wird freigegeben }
end;                           { a, q und z geben ihre Objekte hier frei }
```

* Solche Werte werden **nie kopiert**, sondern verschoben: Zuweisung und Initialisierung leeren
  die Quelle. Aus Konstanten, `is some`-Bindungen und Parametern kann man nicht verschieben, und
  sie dürfen nicht in ein Literal kopiert werden.
* Als Parameter werden sie nur lesend ausgeliehen (per Referenz, wie `T const`); `var`-Parameter
  ändern das Original.
* Objekte, die Felder solcher Typen haben, geben sie mit sich frei; ein unbenutztes
  Funktionsergebnis (`Mach(9);`) wird sofort freigegeben.
* Ein solches Funktionsergebnis muss gespeichert werden, bevor man es benutzt: `Mach(1).Info`
  oder `Zeige(Mach(2))` sind Fehler (sonst würde es nie freigegeben) – `val r := Mach(1)` schreiben.

### Kopieren: `Clone`

Zahlen, Strings, Aufzählungen und Records/Arrays **ohne** Objekte werden bei `b := a` kopiert.
Alles, was Objekte besitzt, wird **verschoben**. Eine Kopie davon gibt es nur ausdrücklich mit
`Clone` (wie `.clone()` in Rust) – geschrieben von Hand oder vom Compiler mit `; auto;`
(wie `#[derive(Clone)]`):

```pascal
type
  TPerson = class
    Name: String;
    Adresse: optional TAdresse;              { besitzt: wird mitgeklont }
    Chef: var optional TPerson;              { Referenz: zeigt weiter auf denselben Chef }
    function Clone: TPerson; const; virtual; auto;
  end;
  TMitarbeiter = class(TPerson)              { erbt Clone; seine Felder werden mitkopiert }
    Buero: optional TAdresse;
  end;
  TPaar = record
    A, B: optional TAdresse;
    function Clone: TPaar; const; auto;
  end;
…
vao kopie := anna.Clone;                     { neues Objekt, kopie besitzt es }
var p: TPerson := mitarbeiter;
vao k := p.Clone;                            { ein TMitarbeiter mit allen Feldern }
val q := paar.Clone;
```

* Der erzeugte `Clone` kopiert alle Werte, klont jedes besitzende Feld (auch in Arrays und
  Records) mit dessen eigenem `Clone` und lässt `var`-Felder auf dasselbe Objekt zeigen.
* Bei Klassen kopiert er das **ganze Objekt seiner tatsächlichen Klasse** – auch Nachfahren, die
  `Clone` nur erben. Hat ein besitzendes Feld (auch in einem Nachfahren) einen Typ ohne `Clone`,
  meldet der Compiler das.
* Kein Konstruktor läuft; das Ergebnis ist ein neuer Besitz, den der Aufrufer übernimmt.
* Form: `function Clone: T; const; auto;` (bei Klassen auch mit `virtual`; das Ergebnis darf eine
  Vorfahrklasse sein). Ein selbst geschriebener `Clone` ist eine gewöhnliche const-Methode.
* **Arrays** haben `Clone` eingebaut: `val d := b.Clone;` kopiert das Array und klont jedes
  enthaltene Objekt (die Elementklasse braucht einen `Clone`); ohne Objekte ist es eine einfache
  Kopie. Wie jeder Wert, der Objekte besitzt, muss das Ergebnis gespeichert werden.

### Heapwerte: Werte in `vao`

Auch Zahlen, Strings, Records und Arrays können einem Besitzer auf dem Heap gehören (wie
`Box<T>` in Rust). Man benutzt sie wie einen normalen Wert; am Ende des Gültigkeitsbereichs
werden sie freigegeben:

```pascal
vao
  I: Integer := 20;            { legt die Heap-Zelle an }
  V: TVektor := default;       { Record auf dem Heap }
begin
  I := I + 1;
  Verdopple(I);                { var-Parameter: Referenz auf den Heapwert }
  V.Skaliere(2);
end;                           { I und V werden hier freigegeben }
```

* Jede Zuweisung speichert einen neuen **Wert** in der Zelle; `J := I` zwischen zwei Heapwerten
  kopiert also den Wert. Nach `I.Free` legt die nächste Zuweisung die Zelle neu an.
* Nützlich vor allem für große Records oder Arrays, die nicht im Stackframe liegen sollen.

### Literale und `default`

```pascal
val
  p: TPunkt := (X: 1; Y: 2);                     { Record-Literal }
  a: array[1..3] of Integer := (10, 20, 30);    { Array-Literal }
  z: TGross := default;                          { alles 0 / '' / none }
```

Literale passen sich dem erwarteten Typ an (Initialisierer, Zuweisung, Argument, `Result`).

## Typ aus dem Initialisierer und Blöcke

Ohne Typangabe bestimmt der Initialisierer den Typ; danach ist er fest:

```pascal
val
  N := 3.5;              { Real }
  S := 'Hallo';          { String }
vao
  I := 5;                { Integer auf dem Heap }
begin
  N := 'x';              { Fehler: N ist Real }
end;
```

* Aus `none` und `default` lässt sich kein Typ ableiten (`val x: optional T := none` schreiben).
* `for val i := 1 to n do …` deklariert die Schleifenvariable direkt in der Schleife.
* `while val i := 0; i < n do …` deklariert eine Variable für die ganze `while`-Schleife: Sie wird
  **einmal** vor der ersten Prüfung angelegt, behält ihren Wert über die Durchläufe und ist nach
  der Schleife nicht mehr sichtbar (auch mit `var` und `vao`; ein `vao` wird nach der Schleife
  freigegeben):

  ```pascal
  while val i := 0; i < 3 do
  begin
    Write(i, ' ');
    Inc(i);
  end;
  ```

**Blöcke mit eigenen Variablen:** Vor einem verschachtelten `begin … end` dürfen `val`-,
`var`-, `vao`- und `const`-Abschnitte stehen. Sie bilden einen eigenen Gültigkeitsbereich – auch
als Schleifenrumpf, dann mit frischen Variablen in jedem Durchlauf:

```pascal
begin
  val j := 10;
  vao s := TFoo.Create(1);   { gehört nur diesem Block }
  begin
    WriteLn(j, ' ', s.V);
  end;                       { s wird hier freigegeben, j ist nicht mehr sichtbar }

  while i < 3 do
    val q := i * 100;
    begin
      WriteLn(q);
      Inc(i);
    end;
end;
```

* Besitzer des Blocks geben am Blockende frei; Ausleihen auf ihre Objekte sind danach
  ungültig (der Borrow Checker meldet ihre Benutzung).
* Routinen, Typen und typisierte Konstanten sind in Blöcken nicht erlaubt.

## Unveränderlich: `const` und `T const`

**`const`** deklariert nur noch Konstanten, die beim Übersetzen feststehen (auch für
Array-Grenzen nutzbar), sowie typisierte Konstanten:

```pascal
const
  Max = 100;
  Name = 'ppc';
  Limit: Integer = 5;          { schreibgeschützt }
```

Für unveränderliche Werte, die erst zur Laufzeit feststehen, gibt es den Typmodifizierer
**`T const`** – hinter dem Typ, analog zu `; const;` bei Methoden:

```pascal
procedure P(n: Integer);
val
  Hoch: Integer const := n * n;          { unveränderlich, Wert zur Laufzeit }
  w: TVektor const := v;                 { unveränderliche Kopie eines Records }
var
  leser: TPerson const := anna;          { nur lesbare Ausleihe eines Objekts }
begin
  Hoch := 1;                             { Fehler }
  w.X := 1;                              { Fehler: Teil einer Konstanten }
  w.Skaliere(2);                         { Fehler: Skaliere ist keine const-Methode }
  WriteLn(w.Laenge);                     { erlaubt: const-Methode }
```

* Unveränderlich ist alles, was im Wert steckt: Record-Felder und Array-Elemente. Auf
  konstanten Records dürfen nur **const-Methoden** aufgerufen werden (siehe unten).
* `var x: Integer const := y` ist eine nur lesende Referenz auf `y`.
* Felder können nicht `const` sein.

**`T const` bei Parametern:** Der Parameter wird **per Referenz** übergeben (keine Kopie) und
ist nur lesbar – wie `const T&` in C++ bzw. `&T` in Rust. Sinnvoll für Strings, Records und
Arrays; Ausdrücke als Argument werden in eine temporäre Kopie gelegt:

```pascal
function Summe(A: TGross const): Integer;           { keine Kopie des Arrays }
procedure Zeige(S: String const; V: TVektor const);
…
WriteLn(Doppelt(Hoch + 1));
```

| Parameter | Übergabe | Ändern erlaubt |
|---|---|---|
| `x: T` | Kopie (Objekte: Ausleihe) | ja, nur die Kopie (Objekte: ja) |
| `x: T const` | Referenz | nein |
| `var x: T` / `out x: T` | Referenz | ja, beim Aufrufer |
| `vao x: TFoo` | Objekt wird übergeben | ja, der Parameter besitzt es |

Der Borrow Checker behandelt `T const`-Parameter als lesende Ausleihe: `Q(x, x)` mit einem
`var`- und einem `T const`-Parameter ist ein Fehler.

### Konstante Methoden und Routinen: `; const;`

`const;` als Direktive am Ende der Deklaration, wie `virtual;` (entspricht `void f() const` in C++).
Die Implementierung wiederholt es: `function TVektor.Laenge: Real; const;`.

```pascal
type
  TVektor = record
    X, Y: Real;
    procedure Skaliere(F: Real);          { darf Self ändern }
    function Laenge: Real; const;          { ändert Self nicht }
  end;
  TKonto = class
    function Beschreibung: String; const; virtual;
  end;

function Quadrat(N: Integer): Integer; const;   { freie Routine ohne Seiteneffekte }
begin Result := N * N; end;
```

* **In einer const-Methode** ist `Self` nur lesbar: keine Zuweisung an Felder (auch bei
  Klassen), und auf `Self` dürfen nur weitere const-Methoden aufgerufen werden.
* **Auf konstanten Werten** – `T const`-Variablen und -Parameter, `Self` in const-Methoden – sind
  nur const-Methoden aufrufbar.
* **Eine freie Routine mit `; const;`** hat keine Seiteneffekte auf den Programmzustand: Sie
  ändert keine globalen oder äußeren Variablen, ruft nur andere const-Routinen auf und ruft auf
  nicht-lokalen Objekten nur const-Methoden auf. Parameter, lokale Variablen und neu erzeugte
  Objekte darf sie frei benutzen; Ausgaben mit `WriteLn` sind erlaubt.
* Deklaration und Implementierung müssen beide `const` sein; ein `override` muss die
  const-Eigenschaft der überschriebenen Methode übernehmen. `ClassName` ist eine const-Methode.

### Nur lesbare Objekt-Referenzen: `TFoo const`

`TFoo const` ist eine Referenz, über die ein Objekt **nur gelesen** werden kann – wie `&T`
gegenüber `&mut T` in Rust:

```pascal
procedure Zeige(P: TPerson const);
function Aelter(A, B: TPerson const): var TPerson const;
function BesterFreund: var optional TPerson const; const;
var
  leser: TPerson const := anna;                 { änderbar -> nur lesbar: erlaubt }
begin
  WriteLn(leser.Info);                          { const-Methode: erlaubt }
  leser.Geburtstag;                             { Fehler: keine const-Methode }
  leser.Alter := 1;                             { Fehler: Objekt ist nur lesbar }
```

* **Nur lesbar ist alles, was man darüber erreicht:** Felder vom Klassentyp
  (`P.Freund is some f` → `f` ist `TPerson const`), Array-Elemente, Ergebnisse von `as`.
* Eine nur lesbare Referenz lässt sich **nicht** in eine änderbare umwandeln – weder durch
  Zuweisung noch als Argument, `var`-Parameter oder Funktionsergebnis.
* `Self` ist in const-Methoden nur lesbar; darum kann eine const-Methode nur
  `var … const`-Referenzen auf ihre Felder herausgeben.

## Generische Typen: `TListe<T>`

Records und Klassen können Typparameter haben (Stufe 1 des Designs in
[docs/generics.md](docs/generics.md)). Für jede benutzte Typkombination erzeugt der Compiler eine
eigene Instanz – geprüft beim Übersetzen und ohne Kosten zur Laufzeit:

```pascal
type
  TPaar<A, B> = record
    Erstes: A;
    Zweites: B;
  end;
  TListe<T> = class
    Elemente: array[1..10] of optional T;
    Anzahl: Integer;
    procedure Add(vao x: T);
    function Hole(i: Integer): var optional T;
  end;
  TKnoten<T> = class
    Wert: T;
    Next: optional TKnoten<T>;               { rekursiv }
  end;
  TKundenListe = class(TListe<TKunde>)       { erbt von einer Instanz }
    function Suche(N: String const): var optional TKunde;
  end;

procedure TListe<T>.Add(vao x: T);          { Methoden wiederholen <T> }
begin
  Inc(Anzahl);
  Elemente[Anzahl] := x;                     { Move, auch wenn T ein Objekt ist }
end;
…
val p: TPaar<String, Integer> := (Erstes: 'Anna'; Zweites: 30);
vao zahlen := TListe<Integer>.Create;
vao kunden := TKundenListe.Create;
kunden.Add(TKunde.Create('Anna'));
if kunden.Suche('Anna') is some k then WriteLn(k.Name);    { k ist ein TKunde }
```

* Generische Typen werden auf Programmebene deklariert; generisch sein können Records, Klassen,
  Funktionen und Prozeduren.

**Generische Funktionen und Prozeduren:**

```pascal
function Max<T>(a, b: T): T;
begin
  if a > b then Result := a else Result := b;
end;

procedure Tausche<T>(var a, b: T);
val h: T := a;
begin a := b; b := h; end;

function Paar<A, B>(x: A; y: B): TPaar<A, B>;
…
WriteLn(Max(3, 7), Max('Anna', 'Bert'));     { T aus den Argumenten: Integer, String }
WriteLn(Max(3, 7.5));                        { Integer und Real: T = Real }
WriteLn(Max<Real>(1, 2));                    { T ausdrücklich }
Tausche(i, j);
val p := Paar('x', 42);                       { TPaar<Char, Integer> }
```

* Die Typargumente werden aus den Argumenten abgeleitet – auch aus `optional T`, Arrays,
  `result T` und generischen Typen (`Erstes<A, B>(p: TPaar<A, B>)`). Geht das nicht (z. B. wenn
  `T` nur im Ergebnis vorkommt), schreibt man sie aus: `Neu<Integer>`.
* Widersprechen sich die Argumente, meldet der Compiler das:
  `cannot infer 'T' of "Max": it is "Integer" from one argument and "Char" from another`.
* Jede Instanz wird einzeln geprüft: `Max` mit einem Record ohne `>` ergibt
  `incompatible types in comparison: "TP" and "TP" (in Max<TP>)`. Rekursion funktioniert.

**Einschränkungen** legen fest, welche Typen eingesetzt werden dürfen (wie in Delphi: Einschränkungen
eines Parameters mit `,`, Parametergruppen mit `;`):

```pascal
function Max<T: Comparable>(a, b: T): T;
function Gleich<T: Equatable>(a, b: T): Boolean;
function Doppel<T: Clone>(x: T): T;           { x.Clone }
type
  TPaar<K: Comparable; V> = record … end;     { K eingeschränkt, V frei }
  TZoo<T: TTier> = class … end;               { T muss TTier oder ein Nachfahre sein }
```

| Einschränkung | erlaubte Typen |
|---|---|
| `class` | Klassen |
| `TBasis` (ein Klassenname) | `TBasis` und Nachfahren |
| `Copy` | Werte ohne Objekte (Zahlen, Strings, Aufzählungen, einfache Records/Arrays) |
| `Clone` | Klassen und Records mit `Clone`, Arrays davon |
| `Equatable` (`=`, `<>`) | Zahlen, Char, Boolean, Strings, Aufzählungen, Klassen (gleiches Objekt) |
| `Comparable` (`<`, `>` …) | Zahlen, Char, Boolean, Strings, Aufzählungen |
| `Numeric` (`+`, `-`, `*`, Zahl-Literale, Vergleiche) | Integer-Typen, Real |

Passt ein Typ nicht, meldet der Compiler das schon beim Aufruf bzw. bei der Instanz:
`"TK" does not satisfy 'T: Comparable' (required by Max<T>)`.

**Der generische Rumpf wird für sich geprüft** (wie in Rust), auch wenn das Generic nie benutzt
wird: Mit einem `T` darf man nur tun, was seine Einschränkungen erlauben. Ohne Einschränkung kann
man es speichern, verschieben, übergeben und in `optional T`, Arrays oder `result T` packen –
aber nicht rechnen, vergleichen oder Felder und Methoden aufrufen:

```pascal
function Summe<T>(a, b: T): T;
begin Result := a + b; end;           { Fehler: operator needs the constraint 'T: Numeric' (in Summe<T>) }

function Name<T>(x: T): String;
begin Result := x.Name; end;          { Fehler: 'Name' is not known for type parameter "T" … }

function Fakultaet<T: Numeric>(n: T): T;
begin
  if n <= 1 then Result := 1 else Result := n * Fakultaet(n - 1);   { ok }
end;
```

Ein Fehler im Rumpf wird so einmal gemeldet – nicht für jede Instanz erneut. Mit einer Basisklasse
als Einschränkung (`T: TTier`) stehen alle Felder und Methoden von `TTier` zur Verfügung, mit
`T: class` die von `TObject`.
* Jede Instanz wird mit den eingesetzten Typen geprüft. Ein Fehler im generischen Code nennt die
  Instanz: `operator not applicable to "TK" and "TK" (in TBox<TK>.Doppelt)`. Auch der Borrow
  Checker prüft jede Instanz.
* Für ein Wert-`T` ist `vao x: T` eine gewöhnliche Übergabe und ein `var`-Ergebnis eine Kopie;
  für ein Objekt-`T` gelten die üblichen Regeln für Besitz und Ausleihen.
* `ClassName` liefert den Namen der Instanz, z. B. `'TListe<TKunde>'`.

## Module: `module` und `import`

Ein Programm bindet Module mit **`import`** ein. Ein Modul ist eine eigene Datei, die mit
**`module Name;`** beginnt, Deklarationen enthält (Konstanten, Typen, Variablen, Routinen,
Generics) und mit `end.` endet:

```pascal
module Geometrie;                       { Datei Geometrie.pas }
import ppc_std;                         { ein Modul darf selbst importieren }

type
  TPunkt = record
    X, Y: Real;
    function Abstand(b: TPunkt): Real; const;
  end;

function TPunkt.Abstand(b: TPunkt): Real; const;
begin Result := Sqrt(Sqr(X - b.X) + Sqr(Y - b.Y)); end;

end.
```

```pascal
program Haupt;                          { Datei Haupt.pas im selben Verzeichnis }
import ppc_std, Geometrie;

val a: TPunkt := (X: 0; Y: 0);
val b: TPunkt := (X: 3; Y: 4);
begin
  WriteLn(a.Abstand(b):0:1);            { 5.0 }
end.
```

* `import Name` sucht `Name.pas` im Verzeichnis des Programms; der Name nach `module` muss zum
  Dateinamen passen. Jedes Modul wird nur einmal geladen, seine eigenen Importe zuerst;
  gegenseitige Importe (Zyklen) sind ein Fehler.
* Alles, was ein Modul deklariert, ist im importierenden Programm sichtbar. Eigene Namen im
  Programm verdecken die eines Moduls.
* Fehler in einem Modul werden mit dessen Dateinamen gemeldet:
  `Geometrie.pas(12,5) Error: …`.
* Ein Modul hat keinen Hauptblock (`begin … end`); es endet mit `end.`. Das alte `uses` ergibt
  `'uses' was replaced by 'import'`.

### Standardbibliothek: `import ppc_std`

Die Standardbibliothek ist selbst ein Perfect-Pascal-Modul ([lib/ppc_std.pas](lib/ppc_std.pas)) und
wird in den Compiler eingebettet. Sie besteht aus generischen Containern und Algorithmen:

```pascal
program Beispiel;
import ppc_std;
val zahlen: array of Integer := (5, 3, 9, 1, 7);
vao kunden := TListe<TKunde>.Create;
vao alter := TMap<String, Integer>.Create;
begin
  Sortiere(zahlen);                                  { 1 3 5 7 9 }
  if Suche(zahlen, 7) is some p then WriteLn(p);     { 3 }
  kunden.Add(TKunde.Create('Anna'));
  if kunden.Hole(0) is some k then WriteLn(k.Name);
  alter.Setze('Anna', 30);
  if alter.Hole('Anna') is some a then WriteLn(a);
end.                                                 { alle Container geben ihre Objekte frei }
```

| Name | Inhalt |
|---|---|
| `TListe<T>` | `Add(vao x)`, `Count`, `Hole(i)` (ausgeliehen), `Nimm(i)` (herausnehmen), `Leeren`; Index ab 0 |
| `TStapel<T>` | `Push(vao x)`, `Pop` (herausnehmen), `Top` (ansehen), `Count`, `IstLeer` |
| `TMap<K: Comparable; V>` | `Setze(k, vao v)` (einfügen/ersetzen), `Hole(k)`, `Enthaelt(k)`, `Entferne(k)`, `Count`; nach Schlüssel sortiert, binäre Suche |
| `Sortiere<T: Comparable>(var a: array of T)` | sortiert aufsteigend (Quicksort) |
| `Suche<T: Equatable>(a: array of T; x: T): optional Integer` | erste Position von `x` oder `none` |

* Die Container besitzen ihre Elemente; `Nimm`, `Pop` und `Entferne` geben den Besitz an den
  Aufrufer weiter (`optional T`), `Hole` und `Top` leihen nur aus.
* Eigene Namen im Programm verdecken die der Bibliothek; die Bibliothek selbst benutzt weiter ihre
  eigenen.
* Ein unbekanntes Modul ergibt `module 'SysUtils' not found (looked for ./SysUtils.pas)`.

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
    function Plus(B: TVektor const): TVektor;   { liefert einen neuen Wert }
    property Breite: Real read X write X;
  end;

procedure TVektor.Skaliere(F: Real); begin X := X * F; Y := Y * F; end;
function TVektor.Plus(B: TVektor const): TVektor; begin Result.Setze(X + B.X, Y + B.Y); end;
…
val
  a: TVektor := (X: 3; Y: 4);
  b: TVektor := default;
  punkte: array[1..100] of TVektor := default;  { 100 Werte hintereinander, keine Allokation }
begin
  b := a;                               { Kopie }
  b.Skaliere(2);                        { a bleibt (3, 4) }
  WriteLn(a.Plus(b).Laenge:0:1);        { Methoden auch auf Ergebniswerten }
end.
```

* In Record-Methoden ist `Self` der Record des Aufrufers: Methoden können ihn ändern.
* Records haben **keine** Vererbung, keine virtuellen Methoden und keine Konstruktoren oder
  Destruktoren; als „Konstruktor“ dient ein Literal, eine Prozedur (`Setze`) oder eine
  Klassenmethode, die den Record zurückgibt (`class function TVektor.Null: TVektor`).
* Records mit Methoden müssen als benannter Typ deklariert werden.

Faustregel: **`record`** für kleine Werte, die kopiert werden (Punkt, Vektor, Datum, Farbe),
**`class`** für Objekte mit Identität, Vererbung oder Polymorphie.

## Borrow Checker

Für Objekte und Referenzen gilt:

| Konstrukt                      | Bedeutung                                         |
|--------------------------------|---------------------------------------------------|
| `vao a := TFoo.Create;`        | `a` **besitzt** das Objekt (Owner)                |
| `var r: TFoo := a;`            | `r` ist eine **Ausleihe** (besitzt nichts)        |
| `var r: TFoo const := a;`      | lesende Ausleihe                                  |
| `var x: Integer := i;`         | Ausleihe der Variablen `i`                        |
| `Next: optional TItem;` (Feld) | Feld besitzt das Objekt (darf fehlen)             |
| `Parent: var TItem;` (Feld)    | Feld ist eine Ausleihe                            |
| `procedure P(x: TFoo)`         | Parameter = Ausleihe (`TFoo const`: lesend)       |
| `procedure P(vao x: TFoo)`     | Parameter übernimmt den Besitz                    |
| `procedure P(var x: TFoo)`     | veränderliche Ausleihe des Besitzer-Platzes       |
| `function F: TFoo`             | Ergebnis wird an den Aufrufer übergeben           |
| `function F: var TFoo`         | Ergebnis ist eine Ausleihe (z. B. `Find`)         |
| `Take(x.Next)`                 | holt den Wert heraus, der Platz wird `none`       |

Regeln (Verstöße sind **Fehler**, die Übersetzung bricht ab):

* `b := a` zwischen zwei Besitzern ist ein **Move**; `a` ist danach unbenutzbar
  (`use of moved value 'a'`), bis es neu zugewiesen wird.
* Kein Zugriff nach `Free`/`Destroy` (`use after free`), kein doppeltes Freigeben.
* Ausleihen auf Objekte lokaler Besitzer dürfen nicht in globale Variablen, Felder oder
  Funktionsergebnisse gelangen – der Besitzer gibt das Objekt am Ende seines Bereichs frei.
* Ein `var`-Ergebnis darf nicht von einem temporären Objekt stammen (`F(TFoo.Create)`).
* Ausleihen dürfen nicht freigegeben und nicht in Besitzer gespeichert werden.
* Eine Ausleihe merkt sich, woher sie stammt. Wird die Quelle freigegeben, verschoben oder
  neu zugewiesen, ist die Ausleihe **hängend**; ihre Benutzung ist ein Fehler.
* Funktionen dürfen keine Referenz auf Objekte lokaler Besitzer zurückgeben.
* Dieselbe Variable darf in einem Aufruf nicht zweimal übergeben werden, wenn einer der
  Parameter `var`/`out` ist (`Swap(x, x)`).
* **Ausleihregeln wie in Rust:** Zu jedem Zeitpunkt gibt es entweder **beliebig viele lesende**
  (`var r: T const`) **oder genau eine änderbare** (`var r: T`) Ausleihe eines Objekts oder
  einer Variablen:
  - Solange eine lesende Ausleihe noch benutzt wird, darf das Objekt nicht geändert werden –
    weder über den Besitzer (Feldzuweisung, nicht-const Methode, `var`-Argument) noch über
    eine andere Ausleihe.
  - Solange eine änderbare Ausleihe noch benutzt wird, darf das Objekt auf keinem anderen Weg
    benutzt (auch nicht gelesen) oder erneut ausgeliehen werden.
  - Wie bei Rusts *non-lexical lifetimes* endet eine Ausleihe mit ihrer **letzten Benutzung**:
    ein Konflikt ist nur ein Fehler, wenn die Ausleihe danach noch benutzt wird. Die Meldung
    nennt die Stelle des Konflikts:
    `borrowed reference 'l' is used here, but 'a' was changed at line 12 while 'l' was borrowing it`.
  - Die Regeln gelten **feldgenau**: eine Ausleihe von `a.Links` verträgt Änderungen an
    `a.Rechts`. Ausleihen, die aus einer anderen abgeleitet sind (`p is some k`, `k.Next`),
    sind mit dieser verträglich.
* Aus einem Feld kann man nicht einfach herausverschieben (`n := h.Next`) – dafür gibt es `Take`.
* `var`-Parameter dürfen beim Verlassen der Routine nicht freigegeben/verschoben zurückbleiben.

Speicherlecks gibt es nicht mehr: Was am Ende eines Bereichs noch besessen wird, gibt der
erzeugte Code frei.

Die Analyse ist flusssensitiv (if/case/Schleifen mit Fixpunkt-Iteration, `Break`/`Continue`/
`Exit`), erkennt `is some`, `x <> none`, `HasValue` und berücksichtigt, welche nicht-lokalen
Variablen aufgerufene Routinen (transitiv) verändern.
Beispiel: [tests/run/ownership.pas](tests/run/ownership.pas), Fehlerbeispiele in
[tests/borrow_fail/](tests/borrow_fail/).

Grenzen der Analyse: Sie arbeitet pro Routine. Felder und Dereferenzierungen werden nicht
einzeln verfolgt (nur ihre Pfade, um Ausleihen ungültig zu machen); die Lebensdauer von
`var`-Feldern wird nicht geprüft; globale Besitzer werden nur im Hauptprogramm verfolgt;
das Verschieben ganzer Arrays/Records, die Objekte besitzen, wird zur Laufzeit durch Leeren
der Quelle abgesichert, nicht im Checker verfolgt.

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
  alter: optional Integer := 42;         { std::optional<int> }
vao
  kopf: optional TKnoten := none;        { Option<Box<Knoten>> }
var
  p: optional TKnoten := none;           { Option<&Knoten> }
begin
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

* **Besitzer ohne Objekt** gibt es nicht: ein nicht-optionaler `vao` muss mit einem Objekt
  initialisiert werden, und nach einem Move oder `Free` ist seine Benutzung ein Fehler.
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

**Array-Grenzen** werden immer geprüft: Ein konstanter Index außerhalb der Grenzen ist ein
Fehler beim Übersetzen (`index 7 is outside the array bounds 1..3`); ein berechneter Index wird
zur Laufzeit geprüft (auch bei Strings, Bereich 0..255):

```
Runtime error 201: range check error at programm.pas(12,9): index 5 is outside 1..3
```

Mit `--no-range-check` entfällt die Prüfung zur Laufzeit (schneller, aber ohne Schutz).

## Fehlerbehandlung: `result T`

Es gibt keine Exceptions. Eine Funktion, die fehlschlagen kann, liefert **`result T`** – einen
Wert vom Typ `T` *oder* einen Fehler, wie `Result<T, E>` in Rust. Der Fehler ist ein `String`;
mit `result T of E` ist er vom Typ `E` (z. B. eine Aufzählung). `result` ohne `T` (bzw.
`result of E`) meldet nur Erfolg oder Fehler, wie `Result<(), E>`.

```pascal
type TFehler = (DurchNull, ZuGross);

function Teile(a, b: Integer): result Integer;
begin
  if b = 0 then Result := error('Division durch 0')
  else Result := a div b;                      { ein Wert ist automatisch ok(...) }
end;

function Pruefe(n: Integer): result of TFehler;
begin
  if n > 100 then Result := error(ZuGross) else Result := ok;
end;

function Rechne(a, b, c: Integer): result Integer;
begin
  val q := try Teile(a, b);                    { Fehler: Rechne liefert ihn sofort zurück }
  begin
    Result := try Teile(q, c);
  end;
end;
…
case Teile(1, 0) of                            { match }
  ok v:    WriteLn('Wert ', v);
  error e: WriteLn('Fehler: ', e);
end;
if r is ok v then WriteLn(v);                  { if let Ok(v) = r }
if Pruefe(5) is ok then …;                     { result ohne Wert }
WriteLn(Teile(7, 0).ValueOr(-1));              { unwrap_or }
if r.IsError then …;                           { is_err; auch r.IsOk }
```

* **Erzeugen:** `ok(v)`, `ok` (ohne Wert), `error(e)`; ein Wert vom Typ `T` wird automatisch
  zu `ok(...)`, Literale und `default` (= `ok` mit dem Nullwert) passen sich an.
* **Auspacken:** `case r of ok v: … error e: … end` (auch mit `else`; der Ausdruck wird nur einmal
  ausgewertet), `r is ok v` / `r is error e` in `if`- und `while`-Bedingungen, `r.ValueOr(d)`,
  `r.IsOk`, `r.IsError`. Ein `result` direkt als Wert zu benutzen ist ein Fehler – er kann nicht
  versehentlich übergangen werden.
* **Weitergeben mit `try`** (wie `?` in Rust): `try r` liefert den Wert eines erfolgreichen
  Ergebnisses; bei einem Fehler endet die Funktion sofort und gibt den Fehler zurück. Besitzer
  (`vao`) werden dabei wie bei `Exit` freigegeben. `try` steht als ganze Anweisung
  (`try Speichere(x);`), als Wert einer Zuweisung oder als Initialisierer und nur in Funktionen,
  die selbst ein `result` mit demselben Fehlertyp liefern.
* Wert und Fehler sind Werte (Zahlen, Strings, Aufzählungen, Records, Arrays); Objekte in einem
  `result` werden noch nicht unterstützt.
* Intern ist ein `result` ein kleines Struct aus Fehler-Flag, Wert und Fehler – ohne Kosten,
  solange kein Fehler auftritt.

Ein vollständiges Beispiel (Bank-Überweisung mit Fehler-Aufzählung, `try`-Kette und allen
Arten des Auspackens): [examples/ueberweisung.pas](examples/ueberweisung.pas).

## Endlosschleife: `loop`

`loop <Anweisung>` wiederholt die Anweisung ohne Bedingung (wie `loop` in Rust); verlassen wird
sie mit `Break`, `Exit` oder einem fehlgeschlagenen `try`:

```pascal
loop
  begin
    ReadLn(zeile);
    if zeile = 'ende' then Break;
    Verarbeite(zeile);
  end;

loop
  vao k := TAuftrag.Create(n);       { frischer Besitzer in jedem Durchlauf }
  begin
    if not k.Bearbeite then Break;   { k wird auch bei Break freigegeben }
  end;
```

Verlässt `Break` oder `Continue` einen Block mit eigenen Variablen, geben dessen Besitzer ihre
Objekte sofort frei (in jeder Schleifenart); Ausleihen auf sie sind danach ungültig.

## Unterstützte Sprache

* `program`, `module` und `import` (eigene Module, Standardbibliothek `ppc_std`), `const` (auch typisierte Konstanten und Array-Initialisierer),
  `type`, `val` (Werte), `var` (Referenzen), `vao` (Besitzer), Blöcke mit eigenen Abschnitten
* Typen: `Integer`/`LongInt` (32 Bit), `ShortInt`, `SmallInt`, `Byte`, `Word`, `Cardinal`,
  `Int64`, `Boolean`, `Char`, `Real`/`Double`, `String` (ShortString, max. 255 Zeichen),
  Aufzählungen, Teilbereiche, statische (auch mehrdimensionale) Arrays, Records (mit Methoden),
  `class` (mit Klassenmethoden)
* Prozeduren/Funktionen, beliebig verschachtelt (Zugriff auf äußere Variablen über
  Static Links), `var`-, `out`-, `vao`- und `T const`-Parameter, `forward`, Rekursion, `Result`/Funktionsname,
  `Exit(Wert)`
* Anweisungen: `:=`, `if`, `case` (auch mit Strings und Bereichen), `while`, `repeat`, `for … to/downto`,
  `loop` (Endlosschleife), `with`, `Break`, `Continue`, `Exit`
* Ausdrücke: Arithmetik, `div`, `mod`, `shl`, `shr`, `and`/`or`/`xor`/`not` (Kurzschluss bei Boolean),
  Vergleiche, Stringverkettung, `in [a, b..c]`, Typumwandlungen `Integer(x)`, `TFoo(obj)`
* Standardroutinen: `Write(Ln)` mit `:Breite:Nachkommastellen`, `Read(Ln)`, `Length`, `Copy`,
  `Pos`, `Delete`, `Insert`, `UpCase`, `UpperCase`, `LowerCase`, `Concat`, `IntToStr`, `StrToInt`,
  `FloatToStr`, `Str`, `SetLength`, `Ord`, `Chr`, `Succ`, `Pred`, `Inc`, `Dec`, `Odd`, `Abs`, `Sqr`,
  `Sqrt`, `Sin`, `Cos`, `ArcTan`, `Exp`, `Ln`, `Trunc`, `Round`, `Frac`, `Int`, `High`, `Low`,
  `SizeOf`, `Random`, `Randomize`, `Halt`, `Assigned`, `FreeAndNil`, `Take`;
  bei `optional`: `HasValue`, `ValueOr`; bei `result`: `IsOk`, `IsError`, `ValueOr`

Nicht unterstützt: `nil` (ersetzt durch `optional`/`none`), Zeiger (`^`, `@`, `New`, `Dispose`,
`Pointer` – ersetzt durch Objekte, `var`-Referenzen und `var`-Parameter), `object`-Typen (ersetzt durch `class`), Mengentypen (außer Mengen-Literalen bei `in`), offene
Array-Parameter (`array of T` ist ein dynamisches Array), Exceptions (ersetzt durch `result T` und `try`), Interfaces, Überladen, Prozedurtypen,
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
| `lib/ppc_std.pas` | Standardbibliothek in Perfect Pascal, wird eingebettet     |
| `tests/`          | Testprogramme und `run.sh`                                 |

Klassen werden als C-Structs mit `__vmt`-Zeiger am Anfang abgebildet. Geerbte Felder liegen
als Präfix vorne, daher sind Zeiger auf Nachfahren mit Zeigern auf Vorfahren layoutkompatibel.
Ein Heapobjekt ist ein mit `calloc` angelegtes Struct; ein Stackobjekt ist dasselbe Struct als
lokale C-Variable, die Pascal-Variable zeigt darauf, und am Blockende ruft der erzeugte Code den
virtuellen Destruktor ohne Freigabe auf. Am Ende eines Bereichs gibt der erzeugte
Code alles frei, was `vao`-Variablen noch besitzen (in umgekehrter Reihenfolge). Eine
`var`-Referenz auf einen Wert ist in C ein Zeiger auf die Original-Variable; Klassenmethoden
sind C-Funktionen ohne `self`-Parameter.
