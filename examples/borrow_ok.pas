program BorrowOk;
{ Ownership-Regeln von ppc an einem kleinen Adressbuch.
  Besitzer:  Variablen im Abschnitt 'var', Felder vom Klassentyp
  Ausleihe:  'borrowed' Variablen, Wert-/const-Parameter
  Move:      Zuweisung Besitzer -> Besitzer, 'owned'-Parameter, Take()
  optional:   nur solche Referenzen duerfen fehlen (none), z. B. das Listenende;
             benutzt werden sie nach dem Auspacken mit 'is some' }
type
  TPerson = class
    Name: String;
    Next: optional TPerson;               { besitzt den Nachfolger, none am Ende }
    constructor Create(const AName: String);
    destructor Destroy; override;
  end;

  TBuch = class
    Erste: optional TPerson;              { besitzt die ganze Kette }
    procedure Neu(owned P: TPerson);     { uebernimmt den Besitz von P }
    function Suche(const N: String): borrowed optional TPerson;  { leiht nur aus }
    function Entnimm: optional TPerson;   { gibt Besitz an den Aufrufer }
    destructor Destroy; override;
  end;

constructor TPerson.Create(const AName: String);
begin inherited Create; Name := AName; end;

destructor TPerson.Destroy;
begin Next.Free; inherited Destroy; end;

procedure TBuch.Neu(owned P: TPerson);
begin
  P.Next := Take(Erste);                 { Erste wird none, Kette wandert zu P.Next }
  Erste := P;                            { Move: P ist danach verbraucht }
end;

function TBuch.Suche(const N: String): borrowed optional TPerson;
val p: borrowed optional TPerson;
begin
  Result := none;
  p := Erste;
  while p is some x do begin
    if x.Name = N then begin Result := x; Exit; end;
    p := x.Next;
  end;
end;

function TBuch.Entnimm: optional TPerson;
begin
  Result := Take(Erste);
  if Result is some r then Erste := Take(r.Next);
end;

destructor TBuch.Destroy;
begin Erste.Free; inherited Destroy; end;

procedure Zeige(B: TBuch);               { Wertparameter = Ausleihe, darf nicht freigeben }
val p: borrowed optional TPerson;
begin
  p := B.Erste;
  while p is some x do begin Write(x.Name, ' '); p := x.Next; end;
  WriteLn;
end;

var
  buch: TBuch;                           { Heapobjekte: Besitzer }
  anna: TPerson;
  weg: optional TPerson;
val
  treffer: borrowed optional TPerson;     { Ausleihe }
begin
  buch := TBuch.Create;
  buch.Neu(TPerson.Create('Bert'));      { Temporaeres Objekt direkt uebergeben }
  anna := TPerson.Create('Anna');
  buch.Neu(anna);                        { Move: anna besitzt nichts mehr }
  anna := TPerson.Create('Carla');       { neu belegen ist erlaubt }
  buch.Neu(anna);
  Zeige(buch);

  treffer := buch.Suche('Anna');         { Ausleihe aus dem Buch }
  if treffer is some t then WriteLn('gefunden: ', t.Name);

  weg := buch.Entnimm;                   { Besitz kommt zurueck }
  if weg is some w then WriteLn('entnommen: ', w.Name);
  weg.Free;                              { Besitzer gibt frei }
  Zeige(buch);

  buch.Free;                             { gibt alle restlichen Personen frei }
end.
