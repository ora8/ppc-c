program StackObjects;
{ stack objects: 'val k := TClass.Create(...)' lives in the stack frame, Destroy at the end of the block }
type
  TFigure = class
    X: Integer;
    constructor Create(AX: Integer);
    destructor Destroy; override;
    procedure Show; virtual;
  end;
  TCircle = class(TFigure)
    R: Real;
    Name: optional TFigure;
    constructor Create(AX: Integer; AR: Real);
    procedure Show; override;
  end;
constructor TFigure.Create(AX: Integer); begin inherited Create; X := AX; end;
destructor TFigure.Destroy; begin WriteLn('  Destroy ', ClassName, ' ', X); inherited Destroy; end;
procedure TFigure.Show; begin WriteLn('Figure ', X); end;
constructor TCircle.Create(AX: Integer; AR: Real); begin inherited Create(AX); R := AR; end;
procedure TCircle.Show; begin WriteLn('Circle ', X, ' r=', R:0:1); end;

procedure Draw(F: TFigure); begin F.Show; end;

procedure Local;
val k := TCircle.Create(1, 2.5);
begin
  Draw(k);
  k.Name := TFigure.Create(99);           { owning field of a stack object }
  var f: TFigure := k;
  begin f.X := 5; end;
  k.Show;
end;

val g: TFigure := TFigure.Create(7);
begin
  Local;
  for val i := 1 to 2 do
    val t := TFigure.Create(i * 10);
    begin
      t.Show;
    end;
  g.Show;
  WriteLn('End');
end.
