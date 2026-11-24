program StackObjects;
{ Stackobjekte: 'val k := TKlasse.Create(...)' liegt im Stackframe, Destroy am Blockende }
type
  TFigur = class
    X: Integer;
    constructor Create(AX: Integer);
    destructor Destroy; override;
    procedure Zeige; virtual;
  end;
  TKreis = class(TFigur)
    R: Real;
    Name: optional TFigur;
    constructor Create(AX: Integer; AR: Real);
    procedure Zeige; override;
  end;
constructor TFigur.Create(AX: Integer); begin inherited Create; X := AX; end;
destructor TFigur.Destroy; begin WriteLn('  Destroy ', ClassName, ' ', X); inherited Destroy; end;
procedure TFigur.Zeige; begin WriteLn('Figur ', X); end;
constructor TKreis.Create(AX: Integer; AR: Real); begin inherited Create(AX); R := AR; end;
procedure TKreis.Zeige; begin WriteLn('Kreis ', X, ' r=', R:0:1); end;

procedure Male(F: TFigur); begin F.Zeige; end;

procedure Lokal;
val k := TKreis.Create(1, 2.5);
begin
  Male(k);
  k.Name := TFigur.Create(99);           { besitzendes Feld eines Stackobjekts }
  var f: TFigur := k;
  begin f.X := 5; end;
  k.Zeige;
end;

val g: TFigur := TFigur.Create(7);
begin
  Lokal;
  for val i := 1 to 2 do
    val t := TFigur.Create(i * 10);
    begin
      t.Zeige;
    end;
  g.Zeige;
  WriteLn('Ende');
end.
