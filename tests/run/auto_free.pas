program AutoFree;
{ Heapobjekte und Heapwerte werden automatisch freigegeben, wenn ihr Besitzer
  den Gueltigkeitsbereich verlaesst – ausser der Besitz wurde weitergegeben. }
type
  TKnoten = class
    Wert: Integer;
    Next: optional TKnoten;            { Felder werden mit dem Objekt freigegeben }
    constructor Create(W: Integer);
    destructor Destroy; override;
  end;
  TListe = class
    Kopf: optional TKnoten;
    procedure Vorne(W: Integer);
    procedure Zeige;
  end;

constructor TKnoten.Create(W: Integer); begin inherited Create; Wert := W; end;
destructor TKnoten.Destroy; begin WriteLn('  frei: ', Wert); inherited Destroy; end;

procedure TListe.Vorne(W: Integer);
var k: TKnoten.Create(W);
begin
  k.Next := Take(Kopf);
  Kopf := k;                           { Besitz wandert in die Liste: k wird nicht freigegeben }
end;

procedure TListe.Zeige;
val p: borrowed optional TKnoten;
begin
  p := Kopf;
  while p is some k do begin Write(k.Wert, ' '); p := k.Next; end;
  WriteLn;
end;

function Baue(n: Integer): TListe;     { Ergebnis: Besitz geht an den Aufrufer }
var l: TListe.Create;
val i;                                 { Typ aus der Schleife: Integer }
begin
  for i := 1 to n do l.Vorne(i);
  Result := l;
end;

procedure Fuelle(var Ziel: TKnoten);   { Rueckgabe ueber var-Parameter }
var neu: TKnoten.Create(77);
begin
  Ziel := neu;                          { altes Ziel wird freigegeben, neu wandert hinaus }
end;

procedure Lokal;
var
  a: TKnoten.Create(1);
  I;
begin
  I := 42;                              { Heapwert }
  WriteLn('Lokal: a = ', a.Wert, ', I = ', I);
end;                                    { a und I werden hier freigegeben }

var
  liste: TListe;
  z: TKnoten.Create(5);
begin
  Lokal;
  WriteLn('-- Liste aus einer Funktion');
  liste := Baue(3);
  liste.Zeige;
  WriteLn('-- Ueberschreiben gibt das alte Objekt frei');
  liste := Baue(1);
  liste.Zeige;
  WriteLn('-- var-Parameter');
  Fuelle(z);
  WriteLn('z = ', z.Wert);
  WriteLn('-- temporaeres Objekt');
  WriteLn(TKnoten.Create(9).Wert);
  WriteLn('-- Programmende');
end.
