program BorrowOk;
{ Ownership-Regeln von ppc an einem kleinen Adressbuch.
  Besitzer:  normale Variablen/Felder vom Klassentyp
  Ausleihe:  'borrowed' Variablen, Wert-/const-Parameter
  Move:      Zuweisung Besitzer -> Besitzer, 'owned'-Parameter, Take() }
type
  TPerson = class
    Name: String;
    Next: TPerson;                       { besitzt den Nachfolger }
    constructor Create(const AName: String);
    destructor Destroy; override;
  end;

  TBuch = class
    Erste: TPerson;                      { besitzt die ganze Kette }
    procedure Neu(owned P: TPerson);     { uebernimmt den Besitz von P }
    function Suche(const N: String): borrowed TPerson;  { leiht nur aus }
    function Entnimm: TPerson;           { gibt Besitz an den Aufrufer }
    destructor Destroy; override;
  end;

constructor TPerson.Create(const AName: String);
begin inherited Create; Name := AName; end;

destructor TPerson.Destroy;
begin Next.Free; inherited Destroy; end;

procedure TBuch.Neu(owned P: TPerson);
begin
  P.Next := Take(Erste);                 { Erste wird nil, Kette wandert zu P.Next }
  Erste := P;                            { Move: P ist danach verbraucht }
end;

function TBuch.Suche(const N: String): borrowed TPerson;
var p: borrowed TPerson;
begin
  Result := nil;
  p := Erste;
  while p <> nil do begin
    if p.Name = N then begin Result := p; Exit; end;
    p := p.Next;
  end;
end;

function TBuch.Entnimm: TPerson;
begin
  Result := Take(Erste);
  if Result <> nil then Erste := Take(Result.Next);
end;

destructor TBuch.Destroy;
begin Erste.Free; inherited Destroy; end;

procedure Zeige(B: TBuch);               { Wertparameter = Ausleihe, darf nicht freigeben }
var p: borrowed TPerson;
begin
  p := B.Erste;
  while p <> nil do begin Write(p.Name, ' '); p := p.Next; end;
  WriteLn;
end;

var
  buch: TBuch;
  anna, weg: TPerson;
  treffer: borrowed TPerson;
begin
  buch := TBuch.Create;
  buch.Neu(TPerson.Create('Bert'));      { Temporaeres Objekt direkt uebergeben }
  anna := TPerson.Create('Anna');
  buch.Neu(anna);                        { Move: anna besitzt nichts mehr }
  anna := TPerson.Create('Carla');       { neu belegen ist erlaubt }
  buch.Neu(anna);
  Zeige(buch);

  treffer := buch.Suche('Anna');         { Ausleihe aus dem Buch }
  if treffer <> nil then WriteLn('gefunden: ', treffer.Name);

  weg := buch.Entnimm;                   { Besitz kommt zurueck }
  WriteLn('entnommen: ', weg.Name);
  weg.Free;                              { Besitzer gibt frei }
  Zeige(buch);

  buch.Free;                             { gibt alle restlichen Personen frei }
end.
