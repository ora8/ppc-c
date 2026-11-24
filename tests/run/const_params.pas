program ConstParams;
{ 'T const': unveraenderlich; als Parameter per Referenz, ohne Kopie }
type
  TVektor = record
    X, Y: Real;
    procedure Skaliere(F: Real);
    function Laenge: Real; const;
  end;
  TGross = array[1..1000] of Integer;
procedure TVektor.Skaliere(F: Real); begin X := X * F; Y := Y * F; end;
function TVektor.Laenge: Real; const; begin Result := Sqrt(X * X + Y * Y); end;

function Summe(A: TGross const): Integer;     { keine Kopie von 4000 Bytes }
begin
  Result := 0;
  for val i := 1 to 1000 do Result := Result + A[i];
end;

procedure Zeige(S: String const; V: TVektor const);
begin
  WriteLn(S, ': ', V.Laenge:0:1);                { Funktionen sind erlaubt }
end;

function Doppelt(N: Integer const): Integer; begin Result := N * 2; end;

procedure P(n: Integer);
const
  Max = 100;                    { Konstante zur Uebersetzungszeit }
val
  Hoch: Integer const := n * n; { unveraenderlich, Wert zur Laufzeit }
  Text: String const := 'n=' + IntToStr(n);
  v: TVektor := default;
begin
  v.X := 3; v.Y := 4;
  WriteLn(Max, ' ', Hoch, ' ', Text);
  Zeige('v', v);
  Zeige('temporaer', v);
  WriteLn('Doppelt(Hoch + 1) = ', Doppelt(Hoch + 1));   { Ausdruck als const-Argument }
  val w: TVektor const := v;    { unveraenderliche Kopie im Block }
  begin
    WriteLn('w.Laenge = ', w.Laenge:0:1);
  end;
end;

val
  g: TGross := default;
begin
  for val i := 1 to 1000 do g[i] := i;
  WriteLn('Summe = ', Summe(g));
  P(7);
end.
