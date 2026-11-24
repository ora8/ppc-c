# Design: Generics für ppc

Status: **abgenommen** (alle Vorschläge aus Abschnitt 8 angenommen). Stufe 1 (generische Records und Klassen), Stufe 2 (generische Funktionen und Prozeduren), Stufe 3 (Einschränkungen) und Stufe 4 (Prüfung des Rumpfs gegen die Einschränkungen) sind gebaut.

## 1. Ziel

Typen und Routinen sollen einmal geschrieben und für beliebige Typen benutzt werden können –
geprüft beim Übersetzen, ohne `as`-Umwandlungen und so schnell wie von Hand geschriebener Code.

```pascal
vao kunden := TListe<TKunde>.Create;
kunden.Add(TKunde.Create('Anna'));
if kunden.Hole(1) is some k then WriteLn(k.Name);   { k ist ein TKunde, kein TObject }

WriteLn(Max(3, 7), ' ', Max('Anna', 'Bert'));        { Max<Integer>, Max<String> }
```

**Nicht-Ziele:** keine Template-Metaprogrammierung wie in C++, keine Spezialisierung für einzelne
Typen, keine Typparameter, die Werte sind (`array[1..N]` mit generischem `N`), kein Überladen.

## 2. Syntax

Im Stil von Delphi / Free Pascal (`<...>` hinter dem Namen):

```pascal
type
  TPaar<A, B> = record                         { generischer Record }
    Erstes: A;
    Zweites: B;
  end;

  TListe<T> = class                            { generische Klasse }
    Elemente: array[1..100] of optional T;
    Anzahl: Integer;
    procedure Add(vao x: T);
    function Hole(i: Integer): var optional T;
    function Laenge: Integer; const;
  end;

  TKundenListe = class(TListe<TKunde>)         { erbt von einer Instanz }
    function Suche(Name: String const): var optional TKunde;
  end;

procedure TListe<T>.Add(vao x: T);             { Methoden wiederholen <T> }
begin
  Inc(Anzahl);
  Elemente[Anzahl] := x;                       { Move, auch wenn T ein Objekt ist }
end;

function Max<T: Comparable>(a, b: T): T;       { generische Funktion mit Einschränkung }
begin
  if a > b then Result := a else Result := b;
end;
```

Benutzung:

```pascal
val p: TPaar<String, Integer> := (Erstes: 'Anna'; Zweites: 30);
vao l := TListe<TKunde>.Create;
val m := Max(3, 7);                            { T aus den Argumenten: Integer }
val r := Max<Real>(3, 7.5);                    { T ausdrücklich angegeben }
```

**Mehrdeutigkeit von `<`:** In Ausdrücken kann `Max<Real>(…)` auch als Vergleich `Max < Real …`
gelesen werden. Der Parser versucht deshalb nach einem Bezeichner zuerst `< Typliste >` gefolgt
von `(`, `.` oder `;` zu lesen und fällt sonst auf den Vergleich zurück (der Parser arbeitet auf
einem Token-Array, ein Zurückspringen ist billig). In Typen gibt es keine Mehrdeutigkeit.

## 3. Einschränkungen (Constraints)

Ohne Einschränkung darf man mit einem `T` nur das tun, was für **jeden** Typ geht: speichern,
verschieben, ausleihen, übergeben, in `optional T` / `array of T` / `result T` verpacken.
Alles andere muss angekündigt werden:

| Einschränkung | erlaubt im Rumpf | passende Typen |
|---|---|---|
| (keine) | speichern, verschieben, ausleihen | alle |
| `T: class` | `T` ist ein Objekt: `ClassName`, `is`, `as`, `Free` | alle Klassen |
| `T: TBasis` | alle Felder und Methoden von `TBasis` | `TBasis` und Nachfahren |
| `T: Copy` | `b := a` kopiert, `a` bleibt gültig | Werte ohne Objekte (Zahlen, Strings, einfache Records) |
| `T: Clone` | `x.Clone` | Klassen/Records mit `Clone`, Arrays davon |
| `T: Equatable` | `=`, `<>` | Zahlen, Strings, Char, Boolean, Aufzählungen |
| `T: Comparable` | zusätzlich `<`, `<=`, `>`, `>=` | Zahlen, Strings, Char, Aufzählungen |
| `T: Numeric` | `+`, `-`, `*`, Zahl-Literale, Vergleiche | Integer-Typen, Real |

Mehrere Einschränkungen: `T: Clone, Comparable`; Parametergruppen mit Einschränkungen werden wie in Delphi mit `;` getrennt: `TMap<K: Comparable; V>`. Die Prüfung der Einschränkungen passiert bei
jeder Instanziierung: `TListe<TKunde>` mit `T: Comparable` ergibt
`"TKunde" does not satisfy 'T: Comparable' (required by TListe<T>)`.

## 4. Besitz und Borrow Checker

Das ist der Teil, der ppc von Delphi unterscheidet. Grundsatz (wie in Rust): **Ein `T` ohne
`Copy` wird im generischen Rumpf immer wie ein Besitzer behandelt.**

```pascal
procedure TListe<T>.Tausche(i, j: Integer);
begin
  val h := Take(Elemente[i]);       { ok: Take funktioniert für optional T }
  Elemente[i] := Take(Elemente[j]);
  Elemente[j] := h;                  { Move }
end;

function Zweimal<T>(a: T): TPaar<T, T>;
begin
  Result := (Erstes: a; Zweites: a); { Fehler: a würde zweimal verschoben – 'T: Copy' oder 'T: Clone' nötig }
end;
```

* Zuweisung von `T` ist ein **Move**; nach dem Move ist die Quelle tot. Mit `T: Copy` ist es eine
  Kopie. So ist der Rumpf für Objekte **und** Werte richtig, und der Borrow Checker meldet
  Fehler schon im generischen Code, nicht erst bei einer bestimmten Instanz.
* `vao x: T` übernimmt den Besitz, `var x: T` ist eine Referenz, `x: T` leiht aus – genau wie
  bei normalen Parametern.
* **Speicherort von `val x: T`** (offene Frage, siehe 8.): Vorschlag: Im generischen Code heißt
  `val x: T` „besitzt ein T an Ort und Stelle“. Für Werte liegt der Wert dort, für Objekte eine
  besitzende Referenz (wie `vao`). Stackobjekte entstehen in generischem Code also nicht.
* Felder vom Typ `T` besitzen ihren Inhalt; bei Objekten muss das Feld `optional T` sein, wie
  heute bei Records (Speicher beginnt leer). Der Compiler verlangt das schon im generischen Typ.
* Automatisches Freigeben, Moves von Arrays/Records mit Objekten und `Clone` funktionieren
  unverändert, weil sie pro Instanz erzeugt werden.

## 5. Prüfung und Übersetzung

**Monomorphisierung:** Für jede benutzte Typkombination erzeugt der Compiler eine eigene Kopie
des Typs bzw. der Routine – wie Rust und C++. `TListe<TKunde>` und `TListe<Integer>` werden zwei
C-Structs mit eigenen Funktionen und eigener VMT. Das kostet zur Laufzeit nichts.

* Namen im C-Code: `S12` mit Kommentar `/* TListe<TKunde> */`; `ClassName` liefert
  `'TListe<TKunde>'`.
* Gleiche Instanzen werden nur einmal erzeugt (Schlüssel: Name + Typargumente).
* Rekursive Instanzen (`TBaum<T>` mit Feld `optional TBaum<T>`) funktionieren über die
  vorhandenen Vorwärtsdeklarationen.
* Unbeschränkte Verschachtelung (`TListe<TListe<TListe<...>>>`) wird nach 50 Ebenen abgebrochen.

**Typprüfung** – zwei Möglichkeiten:

1. **Pro Instanz** (wie C++-Templates): Der Rumpf wird mit eingesetzten Typen geprüft. Einfach zu
   bauen, nutzt die vorhandene Typprüfung fast unverändert. Nachteil: Fehler im generischen Code
   erscheinen erst, wenn er benutzt wird, und die Meldung nennt die Instanz.
2. **Gegen die Einschränkungen** (wie Rust): Der Rumpf wird einmal mit „abstraktem“ `T` geprüft;
   jede Benutzung von `T`, die keine Einschränkung erlaubt, ist sofort ein Fehler. Bessere
   Meldungen, aber deutlich mehr Arbeit in der Typprüfung.

**Umsetzung:** (1) seit Stufe 1; (2) seit Stufe 4: Jedes Generic wird zusätzlich einmal mit
Platzhaltertypen geprüft, die genau das können, was ihre Einschränkungen erlauben (mit einer
Basisklasse als Einschränkung ist der Platzhalter diese Klasse). Dabei entsteht kein Code; ein
Generic mit Fehlern wird für seine Instanzen nicht noch einmal gemeldet. Meldungen in (1) sehen dann so aus:

```
liste.pas(12,5) Error: in TListe<TKunde>.Sortiere: operator '<' is not defined for "TKunde"
                (declare 'T: Comparable' and use a type that supports it)
```

## 6. Voraussetzung: wachsende Arrays

Generische Container sind mit `array[1..100] of optional T` nur halb nützlich – die Größe steht
fest. Für `TListe<T>`, `TStapel<T>` und `TMap<K, V>` braucht es Speicher, der wachsen kann. Zwei
Wege:

* **Dynamische Arrays** `array of T` mit `SetLength(a, n)`, `Length(a)`, Indexprüfung und
  automatischer Freigabe (besitzt seine Elemente wie heute ein festes Array).
* oder nur eine eingebaute generische Klasse `TVektor<T>` (wie Rusts `Vec<T>`), auf der alles
  andere aufbaut.

**Vorschlag:** dynamische Arrays `array of T` als eigene Stufe vor den Containern.

## 7. Ausbaustufen

| Stufe | Inhalt | Ergebnis |
|---|---|---|
| 1 | generische Records und Klassen: Typparameter, Felder, Methoden, explizite Instanziierung `TListe<TKunde>`, Vererbung von Instanzen, Prüfung pro Instanz, Monomorphisierung | `TPaar<A, B>`, `TListe<T>` mit festem Array |
| 2 | generische Funktionen und Prozeduren, Typableitung aus den Argumenten | `Max(3, 7)`, `Tausche<T>(var a, b: T)` |
| 3 | Einschränkungen `class`, Basisklasse, `Copy`, `Clone`, `Equatable`, `Comparable`; Move-Regeln für `T` im Borrow Checker | `Sortiere<T: Comparable>` |
| 4 | Prüfung des generischen Rumpfs gegen die Einschränkungen (Fehler ohne Instanz) | Rust-artige Meldungen |
| 5 | dynamische Arrays `array of T` | wachsende Container |
| 6 | kleine Standardbibliothek: `TListe<T>`, `TStapel<T>`, `TMap<K, V>`, `Sortiere`, `Suche` | als `uses ppc_std` |

Jede Stufe kommt mit eigenen Tests (Lauf- und Fehlertests), ASan-Prüfung und README-Abschnitt.

## 8. Entscheidungen

| Frage | Entscheidung |
|---|---|
| Syntax | `TListe<T>` wie Delphi |
| `val x: T` für Objekt-Instanzen | besitzende Referenz wie `vao`; keine Stackobjekte in generischem Code |
| Prüfstrategie | zuerst pro Instanz, ab Stufe 4 gegen die Einschränkungen |
| Reihenfolge | dynamische Arrays als eigene Stufe vor den Containern |
| Standardbibliothek | als ppc-Quelltext (`uses ppc_std`) |

Ursprüngliche Fragen:

1. **Syntax:** `TListe<T>` wie Delphi (Vorschlag) oder `TListe[T]` (keine Mehrdeutigkeit mit `<`)?
2. **`val x: T` für Objekt-Instanzen:** besitzende Referenz wie `vao` (Vorschlag), oder in
   generischem Code nur `vao`/`var` für `T` erlauben?
3. **Prüfstrategie:** erst pro Instanz, später gegen Einschränkungen (Vorschlag) – oder gleich
   Rust-artig?
4. **Reihenfolge:** dynamische Arrays vor den Generics, danach oder parallel?
5. **Standardbibliothek:** als ppc-Quelltext (`uses ppc_std`) oder fest im Compiler wie `TObject`?
