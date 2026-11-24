program BorrowOk;
{ Ownership-Regeln von ppc an einem kleinen Adressbuch.
  Besitzer:  'vao'-Variablen, Felder vom Klassentyp
  Ausleihe:  'var'-Variablen, Parameter (auch 'T const')
  Move:      Zuweisung Besitzer -> Besitzer, 'vao'-Parameter, Take()
  optional:   nur solche Referenzen duerfen fehlen (none), z. B. das Listenende;
             benutzt werden sie nach dem Auspacken mit 'is some' }
type
  TPerson = class
    Name: String;
    Next: optional TPerson;               { besitzt den Nachfolger, none am Ende }
    constructor Create(AName: String const);
    destructor Destroy; override;
  end;

  TBuch = class
    Erste: optional TPerson;              { besitzt die ganze Kette }
    procedure Neu(vao P: TPerson);     { uebernimmt den Besitz von P }
    function Suche(N: String const): var optional TPerson;  { leiht nur aus }
    function Entnimm: optional TPerson;   { gibt Besitz an den Aufrufer }
    destructor Destroy; override;
  end;

constructor TPerson.Create(AName: String const);
begin inherited Create; Name := AName; end;

destructor TPerson.Destroy;
begin Next.Free; inherited Destroy; end;

procedure TBuch.Neu(vao P: TPerson);
begin
  P.Next := Take(Erste);                 { Erste wird none, Kette wandert zu P.Next }
  Erste := P;                            { Move: P ist danach verbraucht }
end;

function TBuch.Suche(N: String const): var optional TPerson;
var p: optional TPerson := none;
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
var p: optional TPerson := none;
begin
  p := B.Erste;
  while p is some x do begin Write(x.Name, ' '); p := x.Next; end;
  WriteLn;
end;

vao
  buch := TBuch.Create;                  { Besitzer }
  anna: TPerson := TPerson.Create('Anna');
  weg: optional TPerson := none;
var
  treffer: optional TPerson := none;     { Ausleihe }
begin
  
  buch.Neu(TPerson.Create('Bert'));      { Temporaeres Objekt direkt uebergeben }
  
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
