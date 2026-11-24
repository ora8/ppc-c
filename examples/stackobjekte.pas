program StackObjekte;
{ Stackobjekte: Klassenvariablen im Abschnitt 'val' sind Objekte im
  Stackframe. Sie werden mit x.Create(...) konstruiert, brauchen kein Free
  und werden am Ende des Blocks automatisch abgebaut (Destroy).
  Klassenvariablen im Abschnitt 'var' sind Heapobjekte: Erzeugen mit
  x := TKlasse.Create(...), Freigeben mit x.Free. }
type
  TPunkt = class
    X, Y: Integer;
    constructor Create(AX, AY: Integer);
    procedure Verschiebe(DX, DY: Integer);
    function AlsText: String;
  end;

  TFigur = class
    Pos: TPunkt;                    { nicht-optional Feld: muss im Konstruktor gesetzt werden }
    constructor Create(AX, AY: Integer);
    destructor Destroy; override;
    function Name: String; virtual;
    function Flaeche: Real; virtual;
    procedure Zeige;
  end;

  TKreis = class(TFigur)
    Radius: Real;
    constructor Create(AX, AY: Integer; R: Real);
    function Name: String; override;
    function Flaeche: Real; override;
  end;

  TQuadrat = class(TFigur)
    Seite: Integer;
    constructor Create(AX, AY, S: Integer);
    function Name: String; override;
    function Flaeche: Real; override;
  end;

{ ---------------------------------------------------------- TPunkt }
constructor TPunkt.Create(AX, AY: Integer); begin inherited Create; X := AX; Y := AY; end;
procedure TPunkt.Verschiebe(DX, DY: Integer); begin X := X + DX; Y := Y + DY; end;
function TPunkt.AlsText: String;
begin Result := '(' + IntToStr(X) + ',' + IntToStr(Y) + ')'; end;

{ ---------------------------------------------------------- TFigur }
constructor TFigur.Create(AX, AY: Integer);
begin
  inherited Create;
  Pos := TPunkt.Create(AX, AY);     { die Figur besitzt ihren Punkt }
end;
destructor TFigur.Destroy;
begin
  WriteLn('  ', Name, ' wird abgebaut');
  Pos.Free;
  inherited Destroy;
end;
function TFigur.Name: String; begin Result := 'Figur'; end;
function TFigur.Flaeche: Real; begin Result := 0; end;
procedure TFigur.Zeige;
begin
  WriteLn(Name:8, ' bei ', Pos.AlsText, '  Flaeche = ', Flaeche:0:2);
end;

{ ---------------------------------------------------------- TKreis }
constructor TKreis.Create(AX, AY: Integer; R: Real);
begin inherited Create(AX, AY); Radius := R; end;
function TKreis.Name: String; begin Result := 'Kreis'; end;
function TKreis.Flaeche: Real; begin Result := Pi * Radius * Radius; end;

{ ---------------------------------------------------------- TQuadrat }
constructor TQuadrat.Create(AX, AY, S: Integer);
begin inherited Create(AX, AY); Seite := S; end;
function TQuadrat.Name: String; begin Result := 'Quadrat'; end;
function TQuadrat.Flaeche: Real; begin Result := Seite * Seite; end;

{ Polymorphie: nimmt jede Figur, egal ob auf Stack oder Heap }
procedure Bewege(F: TFigur; DX, DY: Integer);
begin
  F.Pos.Verschiebe(DX, DY);
  F.Zeige;                          { ruft die virtuelle Methode des echten Typs }
end;

procedure Lokal;
val
  k: TKreis;                        { Stackobjekt: lebt nur waehrend Lokal laeuft }
begin
  k.Create(0, 0, 1);
  k.Zeige;
end;                                { k.Destroy wird hier automatisch gerufen }

val
  k: TKreis;                        { Stackobjekte des Hauptprogramms }
  q: TQuadrat;
var
  h: TFigur;                        { Heapobjekt derselben Klassenhierarchie }
begin
  WriteLn('--- Stackobjekte ---');
  k.Create(3, 4, 2.0);
  q.Create(0, 0, 5);
  k.Zeige;
  q.Zeige;
  Bewege(k, 1, 1);
  Bewege(q, -1, -1);

  WriteLn('--- lokales Stackobjekt ---');
  Lokal;

  WriteLn('--- Heapobjekt ---');
  h := TQuadrat.Create(7, 7, 2);
  Bewege(h, 1, 0);
  h.Free;                           { Heap: hier frueh freigeben (sonst am Programmende automatisch) }

  WriteLn('--- Programmende: k und q werden automatisch abgebaut ---');
end.
