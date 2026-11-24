{ ppc_std: Standardbibliothek von Perfect Pascal  (einbinden mit 'uses ppc_std;')

  TListe<T>                 Liste, die mitwächst (Index ab 0)
  TStapel<T>                Stapel (zuletzt hinein, zuerst heraus)
  TMap<K: Comparable; V>    Zuordnung Schlüssel -> Wert, nach Schlüssel sortiert
  Sortiere<T: Comparable>   sortiert ein 'array of T' aufsteigend
  Suche<T: Equatable>       Position eines Werts in einem 'array of T' oder none

  Alle Container besitzen ihre Elemente (Objekte werden mit dem Container freigegeben).
  Herausnehmen (Nimm, Pop, Entferne) gibt den Besitz an den Aufrufer weiter. }

type
  TListe<T> = class
    Elemente: array of optional T;
    procedure Add(vao x: T);                       { hinten anhängen, übernimmt x }
    function Count: Integer; const;
    function Hole(i: Integer): var optional T;     { Element i (Objekte: ausgeliehen) }
    function Nimm(i: Integer): optional T;         { Element i herausnehmen }
    procedure Leeren;                              { alle Elemente freigeben }
  end;

  TStapel<T> = class
    Elemente: array of optional T;
    procedure Push(vao x: T);
    function Pop: optional T;                      { oberstes Element herausnehmen }
    function Top: var optional T;                  { oberstes Element ansehen }
    function Count: Integer; const;
    function IstLeer: Boolean; const;
  end;

  TMap<K: Comparable; V> = class
    Schluessel: array of K;                        { aufsteigend sortiert }
    Werte: array of optional V;
    function Position(k: K): Integer; const;       { erste Stelle mit Schluessel >= k }
    procedure Setze(k: K; vao v: V);               { einfügen oder ersetzen }
    function Hole(k: K): var optional V;
    function Enthaelt(k: K): Boolean; const;
    function Entferne(k: K): optional V;           { herausnehmen }
    function Count: Integer; const;
  end;

{ ------------------------------------------------------------------ TListe<T> }

procedure TListe<T>.Add(vao x: T);
begin
  SetLength(Elemente, Length(Elemente) + 1);
  Elemente[High(Elemente)] := x;
end;

function TListe<T>.Count: Integer; const;
begin
  Result := Length(Elemente);
end;

function TListe<T>.Hole(i: Integer): var optional T;
begin
  Result := Elemente[i];
end;

function TListe<T>.Nimm(i: Integer): optional T;
begin
  Result := Take(Elemente[i]);
  for val j := i to High(Elemente) - 1 do Elemente[j] := Take(Elemente[j + 1]);
  SetLength(Elemente, Length(Elemente) - 1);
end;

procedure TListe<T>.Leeren;
begin
  SetLength(Elemente, 0);
end;

{ ----------------------------------------------------------------- TStapel<T> }

procedure TStapel<T>.Push(vao x: T);
begin
  SetLength(Elemente, Length(Elemente) + 1);
  Elemente[High(Elemente)] := x;
end;

function TStapel<T>.Pop: optional T;
begin
  Result := none;
  if Length(Elemente) > 0 then
  begin
    Result := Take(Elemente[High(Elemente)]);
    SetLength(Elemente, Length(Elemente) - 1);
  end;
end;

function TStapel<T>.Top: var optional T;
begin
  Result := none;
  if Length(Elemente) > 0 then Result := Elemente[High(Elemente)];
end;

function TStapel<T>.Count: Integer; const;
begin
  Result := Length(Elemente);
end;

function TStapel<T>.IstLeer: Boolean; const;
begin
  Result := Length(Elemente) = 0;
end;

{ ------------------------------------------------------------------- TMap<K, V> }

function TMap<K, V>.Position(k: K): Integer; const;
val
  lo := 0;
  hi := Length(Schluessel);
begin
  while lo < hi do                                 { binäre Suche }
    val m := (lo + hi) div 2;
    begin
      if Schluessel[m] < k then lo := m + 1 else hi := m;
    end;
  Result := lo;
end;

procedure TMap<K, V>.Setze(k: K; vao v: V);
val i := Position(k);
begin
  if (i < Length(Schluessel)) and (Schluessel[i] = k) then
    Werte[i] := v                                  { ersetzen: der alte Wert wird freigegeben }
  else
  begin
    SetLength(Schluessel, Length(Schluessel) + 1);
    SetLength(Werte, Length(Werte) + 1);
    for val j := High(Schluessel) downto i + 1 do
    begin
      Schluessel[j] := Schluessel[j - 1];
      Werte[j] := Take(Werte[j - 1]);
    end;
    Schluessel[i] := k;
    Werte[i] := v;
  end;
end;

function TMap<K, V>.Hole(k: K): var optional V;
val i := Position(k);
begin
  Result := none;
  if (i < Length(Schluessel)) and (Schluessel[i] = k) then Result := Werte[i];
end;

function TMap<K, V>.Enthaelt(k: K): Boolean; const;
val i := Position(k);
begin
  Result := (i < Length(Schluessel)) and (Schluessel[i] = k);
end;

function TMap<K, V>.Entferne(k: K): optional V;
val i := Position(k);
begin
  Result := none;
  if (i < Length(Schluessel)) and (Schluessel[i] = k) then
  begin
    Result := Take(Werte[i]);
    for val j := i to High(Schluessel) - 1 do
    begin
      Schluessel[j] := Schluessel[j + 1];
      Werte[j] := Take(Werte[j + 1]);
    end;
    SetLength(Schluessel, Length(Schluessel) - 1);
    SetLength(Werte, Length(Werte) - 1);
  end;
end;

function TMap<K, V>.Count: Integer; const;
begin
  Result := Length(Schluessel);
end;

{ --------------------------------------------------------------- Sortieren, Suchen }

procedure SortiereBereich<T: Comparable>(var a: array of T; lo, hi: Integer);
val
  i := lo;
  j := hi;
begin
  if lo >= hi then Exit;
  val pivot: T := a[(lo + hi) div 2];
  begin
    while i <= j do                                { Quicksort }
    begin
      while a[i] < pivot do Inc(i);
      while pivot < a[j] do Dec(j);
      if i <= j then
      begin
        val h: T := a[i];
        begin
          a[i] := a[j];
          a[j] := h;
        end;
        Inc(i);
        Dec(j);
      end;
    end;
  end;
  SortiereBereich(a, lo, j);
  SortiereBereich(a, i, hi);
end;

procedure Sortiere<T: Comparable>(var a: array of T);
begin
  SortiereBereich(a, 0, High(a));
end;

function Suche<T: Equatable>(a: array of T; x: T): optional Integer;
begin
  Result := none;
  for val i := 0 to High(a) do
    if (a[i] = x) and not Result.HasValue then Result := i;
end;
