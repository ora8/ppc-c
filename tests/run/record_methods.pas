program RecordMethods;
{ Records mit Methoden: Werttypen mit Verhalten, ohne Klasse und Heap }
type
  TVektor = record
    X, Y: Real;
    procedure Setze(AX, AY: Real);
    procedure Skaliere(F: Real);                 { aendert den Record selbst }
    function Laenge: Real;
    function Plus(const B: TVektor): TVektor;    { liefert einen neuen Wert }
    function AlsText: String;
    property Breite: Real read X write X;
  end;

  TRechteck = record
    Ecke, Groesse: TVektor;                       { Records in Records }
    function Flaeche: Real;
    function Enthaelt(const P: TVektor): Boolean;
  end;

procedure TVektor.Setze(AX, AY: Real); begin X := AX; Y := AY; end;
procedure TVektor.Skaliere(F: Real); begin X := X * F; Y := Y * F; end;
function TVektor.Laenge: Real; begin Result := Sqrt(Sqr(X) + Sqr(Y)); end;
function TVektor.Plus(const B: TVektor): TVektor;
begin Result.Setze(X + B.X, Y + B.Y); end;
function TVektor.AlsText: String;
val sx, sy: String;
begin
  Str(X:0:1, sx); Str(Y:0:1, sy);
  Result := '(' + sx + ', ' + sy + ')';
end;

function TRechteck.Flaeche: Real; begin Result := Groesse.X * Groesse.Y; end;
function TRechteck.Enthaelt(const P: TVektor): Boolean;
begin
  Result := (P.X >= Ecke.X) and (P.X <= Ecke.X + Groesse.X) and
            (P.Y >= Ecke.Y) and (P.Y <= Ecke.Y + Groesse.Y);
end;

function Vektor(AX, AY: Real): TVektor;          { "Konstruktor" als Funktion }
begin Result.Setze(AX, AY); end;

val
  a, b, c: TVektor;
  r: TRechteck;
  punkte: array[1..3] of TVektor;
  i: Integer;
begin
  a.Setze(3, 4);
  WriteLn('a = ', a.AlsText, ', Laenge ', a.Laenge:0:1);
  b := a;                                         { Kopie }
  b.Skaliere(2);
  WriteLn('a = ', a.AlsText, ', b = ', b.AlsText, '  (b ist eine Kopie)');
  c := a.Plus(b);
  WriteLn('a + b = ', c.AlsText);
  WriteLn('Laenge von Vektor(6, 8): ', Vektor(6, 8).Laenge:0:1);   { Methode auf einem Ergebniswert }
  WriteLn('a.Plus(b).Plus(a) = ', a.Plus(b).Plus(a).AlsText);
  a.Breite := 10;
  WriteLn('nach a.Breite := 10: ', a.AlsText, ', Breite ', a.Breite:0:1);

  r.Ecke.Setze(0, 0);
  r.Groesse.Setze(4, 3);
  WriteLn('Flaeche ', r.Flaeche:0:1, ', enthaelt (2,2): ', r.Enthaelt(Vektor(2, 2)), ', enthaelt (5,1): ', r.Enthaelt(Vektor(5, 1)));

  for i := 1 to 3 do punkte[i].Setze(i, i * i);
  for i := 1 to 3 do punkte[i].Skaliere(10);
  for i := 1 to 3 do Write(punkte[i].AlsText, ' ');
  WriteLn;
  with r.Groesse do begin Skaliere(2); WriteLn('mit with: ', AlsText); end;
end.
