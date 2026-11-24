program ConstMethods;
{ Methoden mit '; const;': aendern Self nicht;
  und Routinen ohne Seiteneffekte auf globale Variablen }
type
  TVektor = record
    X, Y: Real;
    procedure Skaliere(F: Real);
    function Laenge: Real; const;               { aendert Self nicht }
    function Normiert: TVektor; const;
    procedure Zeige; const;
  end;

  TKonto = class
  private
    FSaldo: Integer;
  public
    constructor Create(S: Integer);
    procedure Einzahlen(B: Integer);
    function Saldo: Integer; const;
    function Beschreibung: String; const; virtual;
  end;
  TSparkonto = class(TKonto)
    function Beschreibung: String; const; override; { Override bleibt const }
  end;

procedure TVektor.Skaliere(F: Real); begin X := X * F; Y := Y * F; end;
function TVektor.Laenge: Real; const; begin Result := Sqrt(X * X + Y * Y); end;
function TVektor.Normiert: TVektor; const;
val l := Laenge;                                 { const Methode ruft const Methode }
begin
  Result.X := X / l;
  Result.Y := Y / l;
end;
procedure TVektor.Zeige; const; begin WriteLn('(', X:0:2, ', ', Y:0:2, ') Laenge ', Laenge:0:2); end;

constructor TKonto.Create(S: Integer); begin inherited Create; FSaldo := S; end;
procedure TKonto.Einzahlen(B: Integer); begin FSaldo := FSaldo + B; end;
function TKonto.Saldo: Integer; const; begin Result := FSaldo; end;
function TKonto.Beschreibung: String; const; begin Result := ClassName + ' ' + IntToStr(Saldo); end;
function TSparkonto.Beschreibung: String; const; begin Result := 'Spar: ' + inherited Beschreibung; end;

{ freie const Funktion: nur Parameter und lokale Variablen, keine Seiteneffekte }
function Quadrat(N: Integer): Integer; const; begin Result := N * N; end;
function SummeQuadrate(N: Integer): Integer; const;
begin
  Result := 0;
  for val i := 1 to N do Result := Result + Quadrat(i);   { ruft nur const Routinen }
end;

procedure Ausgabe(V: TVektor const);         { T const: nur const Methoden }
begin
  V.Zeige;
  V.Normiert.Zeige;
end;

val
  v: TVektor := default;
vao
  k := TSparkonto.Create(100);
begin
  v.X := 3; v.Y := 4;
  Ausgabe(v);
  v.Skaliere(2);                                { v selbst ist nicht konstant }
  v.Zeige;
  k.Einzahlen(50);
  WriteLn(k.Beschreibung, ', Saldo ', k.Saldo);
  WriteLn('SummeQuadrate(4) = ', SummeQuadrate(4));
end.
