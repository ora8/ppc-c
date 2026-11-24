program ConstRef;
type
  TVektor = record
    X, Y: Real;
    procedure Skaliere(F: Real);
    const function Laenge: Real;
  end;
  TGross = array[1..1000] of Integer;
procedure TVektor.Skaliere(F: Real); begin X := X * F; Y := Y * F; end;
const function TVektor.Laenge: Real; begin Result := Sqrt(X * X + Y * Y); end;

function Summe(constref A: TGross): Integer;     { keine Kopie von 4000 Bytes }
val i;
begin
  Result := 0;
  for i := 1 to 1000 do Result := Result + A[i];
end;

procedure Zeige(constref S: String; constref V: TVektor);
begin
  WriteLn(S, ': ', V.Laenge:0:1);                { Funktionen sind erlaubt }
end;

function Doppelt(constref N: Integer): Integer; begin Result := N * 2; end;

procedure P(n: Integer);
const
  Max = 100;                    { Konstante zur Uebersetzungszeit }
  Hoch = n * n;                 { unveraenderlich, Wert zur Laufzeit }
  Text = 'n=' + IntToStr(n);
val
  v: TVektor;
begin
  v.X := 3; v.Y := 4;
  WriteLn(Max, ' ', Hoch, ' ', Text);
  Zeige('v', v);
  Zeige('temporaer', v);
  WriteLn('Doppelt(Hoch + 1) = ', Doppelt(Hoch + 1));   { Ausdruck als constref-Argument }
  const w = v;                  { unveraenderliche Kopie im Block }
  begin
    WriteLn('w.Laenge = ', w.Laenge:0:1);
  end;
end;

val
  g: TGross;
  i;
begin
  for i := 1 to 1000 do g[i] := i;
  WriteLn('Summe = ', Summe(g));
  P(7);
end.
