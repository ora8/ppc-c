program class_references;
type
  TShape = class
    Size: Integer;
    constructor Create(ASize: Integer); virtual;
    function Area: Integer; virtual;
    class function Kind: String;
  end;
  TShapeClass = class of TShape;
  TSquare = class(TShape)
    constructor Create(ASize: Integer); override;
    function Area: Integer; override;
  end;
  TCircle = class(TShape)
    function Area: Integer; override;
  end;
constructor TShape.Create(ASize: Integer); begin Size := ASize; end;
function TShape.Area: Integer; begin Result := 0; end;
class function TShape.Kind: String; begin Result := 'shape'; end;
constructor TSquare.Create(ASize: Integer); begin inherited Create(ASize); WriteLn('square ', ASize, ' created'); end;
function TSquare.Area: Integer; begin Result := Size * Size; end;
function TCircle.Area: Integer; begin Result := 3 * Size * Size; end;

function Make(c: TShapeClass; n: Integer): TShape;
begin
  Result := c.Create(n);
end;

val
  kinds: array of TShapeClass := (TSquare, TCircle, TShape);
  c: TShapeClass := TCircle;
begin
  for val k in kinds do
  begin
    vao s := Make(k, 2);
    begin
      WriteLn(k.ClassName, ' ', s.ClassName, ' ', s.Area, ' ', s.ClassType = k, ' ', s is TCircle);
    end;
  end;
  vao x := c.Create(3);
  begin
    WriteLn(x.Area, ' ', x is c, ' ', c.Kind, ' ', c = TCircle, ' ', c <> TSquare);
    c := x.ClassType;
    WriteLn(c.ClassName);
  end;
end.
