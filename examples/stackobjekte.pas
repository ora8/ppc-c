program StackObjekte;
{ Stackobjekte: 'object'-Typen sind Werte. Sie liegen im Stackframe,
  brauchen weder New noch Free und werden beim Zuweisen kopiert. }
type
  TPunkt = object
    X, Y: Integer;
    procedure Setze(AX, AY: Integer);
    procedure Verschiebe(DX, DY: Integer);
    function AlsText: String;
  end;

  TFigur = object
    Pos: TPunkt;                    { Objekt als Feld: eingebettet, kein Zeiger }
    constructor Init(AX, AY: Integer);
    function Name: String; virtual;
    function Flaeche: Real; virtual;
    procedure Zeige;
  end;

  TKreis = object(TFigur)
    Radius: Real;
    constructor Init(AX, AY: Integer; R: Real);
    function Name: String; virtual;
    function Flaeche: Real; virtual;
  end;

  TQuadrat = object(TFigur)
    Seite: Integer;
    constructor Init(AX, AY, S: Integer);
    function Name: String; virtual;
    function Flaeche: Real; virtual;
  end;

{ ---------------------------------------------------------- TPunkt }
procedure TPunkt.Setze(AX, AY: Integer); begin X := AX; Y := AY; end;
procedure TPunkt.Verschiebe(DX, DY: Integer); begin X := X + DX; Y := Y + DY; end;
function TPunkt.AlsText: String;
begin Result := '(' + IntToStr(X) + ',' + IntToStr(Y) + ')'; end;

{ ---------------------------------------------------------- TFigur }
constructor TFigur.Init(AX, AY: Integer); begin Pos.Setze(AX, AY); end;
function TFigur.Name: String; begin Result := 'Figur'; end;
function TFigur.Flaeche: Real; begin Result := 0; end;
procedure TFigur.Zeige;
begin
  WriteLn(Name:8, ' bei ', Pos.AlsText, '  Flaeche = ', Flaeche:0:2);
end;

{ ---------------------------------------------------------- TKreis }
constructor TKreis.Init(AX, AY: Integer; R: Real);
begin inherited Init(AX, AY); Radius := R; end;
function TKreis.Name: String; begin Result := 'Kreis'; end;
function TKreis.Flaeche: Real; begin Result := Pi * Radius * Radius; end;

{ ---------------------------------------------------------- TQuadrat }
constructor TQuadrat.Init(AX, AY, S: Integer);
begin inherited Init(AX, AY); Seite := S; end;
function TQuadrat.Name: String; begin Result := 'Quadrat'; end;
function TQuadrat.Flaeche: Real; begin Result := Seite * Seite; end;

{ Polymorphie: var-Parameter akzeptiert jeden Nachfahren von TFigur }
procedure Bewege(var F: TFigur; DX, DY: Integer);
begin
  F.Pos.Verschiebe(DX, DY);
  F.Zeige;                          { ruft die virtuelle Methode des echten Typs }
end;

{ Rueckgabe eines Stackobjekts als Wert (Kopie) }
function Mitte(const A, B: TPunkt): TPunkt;
begin
  Result.Setze((A.X + B.X) div 2, (A.Y + B.Y) div 2);
end;

procedure Lokal;
var k: TKreis;                      { lebt nur waehrend Lokal laeuft }
begin
  k.Init(0, 0, 1);
  k.Zeige;
end;                                { k verschwindet hier automatisch }

var
  a, b, m: TPunkt;
  k: TKreis;
  q: TQuadrat;
  feld: array[1..3] of TPunkt;      { Array von Stackobjekten }
  i: Integer;
begin
  WriteLn('--- Werte-Semantik ---');
  a.Setze(1, 2);
  b := a;                           { Kopie, kein gemeinsamer Zeiger! }
  b.Verschiebe(10, 10);
  WriteLn('a = ', a.AlsText, '   b = ', b.AlsText);
  m := Mitte(a, b);
  WriteLn('Mitte = ', m.AlsText);

  WriteLn('--- Vererbung und virtuelle Methoden ---');
  k.Init(3, 4, 2.0);
  q.Init(0, 0, 5);
  k.Zeige;
  q.Zeige;
  Bewege(k, 1, 1);
  Bewege(q, -1, -1);

  WriteLn('--- lokales Objekt in einer Prozedur ---');
  Lokal;

  WriteLn('--- Array von Objekten ---');
  for i := 1 to 3 do feld[i].Setze(i, i * i);
  for i := 1 to 3 do Write(feld[i].AlsText, ' ');
  WriteLn;
end.
