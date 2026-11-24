program ConstMethods;
{ const function / const procedure: Methoden, die Self nicht aendern,
  und Routinen ohne Seiteneffekte auf globale Variablen }
type
  TVektor = record
    X, Y: Real;
    procedure Skaliere(F: Real);
    const function Laenge: Real;               { aendert Self nicht }
    const function Normiert: TVektor;
    const procedure Zeige;
  end;

  TKonto = class
  private
    FSaldo: Integer;
  public
    constructor Create(S: Integer);
    procedure Einzahlen(B: Integer);
    const function Saldo: Integer;
    const function Beschreibung: String; virtual;
  end;
  TSparkonto = class(TKonto)
    const function Beschreibung: String; override; { Override bleibt const }
  end;

procedure TVektor.Skaliere(F: Real); begin X := X * F; Y := Y * F; end;
const function TVektor.Laenge: Real; begin Result := Sqrt(X * X + Y * Y); end;
const function TVektor.Normiert: TVektor;
val l = Laenge;                                 { const Methode ruft const Methode }
begin
  Result.X := X / l;
  Result.Y := Y / l;
end;
const procedure TVektor.Zeige; begin WriteLn('(', X:0:2, ', ', Y:0:2, ') Laenge ', Laenge:0:2); end;

constructor TKonto.Create(S: Integer); begin inherited Create; FSaldo := S; end;
procedure TKonto.Einzahlen(B: Integer); begin FSaldo := FSaldo + B; end;
const function TKonto.Saldo: Integer; begin Result := FSaldo; end;
const function TKonto.Beschreibung: String; begin Result := ClassName + ' ' + IntToStr(Saldo); end;
const function TSparkonto.Beschreibung: String; begin Result := 'Spar: ' + inherited Beschreibung; end;

{ freie const Funktion: nur Parameter und lokale Variablen, keine Seiteneffekte }
const function Quadrat(N: Integer): Integer; begin Result := N * N; end;
const function SummeQuadrate(N: Integer): Integer;
val i;
begin
  Result := 0;
  for i := 1 to N do Result := Result + Quadrat(i);   { ruft nur const Routinen }
end;

procedure Ausgabe(constref V: TVektor);         { constref: nur const Methoden }
begin
  V.Zeige;
  V.Normiert.Zeige;
end;

val
  v: TVektor;
var
  k: TSparkonto.Create(100);
begin
  v.X := 3; v.Y := 4;
  Ausgabe(v);
  v.Skaliere(2);                                { v selbst ist nicht konstant }
  v.Zeige;
  k.Einzahlen(50);
  WriteLn(k.Beschreibung, ', Saldo ', k.Saldo);
  WriteLn('SummeQuadrate(4) = ', SummeQuadrate(4));
end.
