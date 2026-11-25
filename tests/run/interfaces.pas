program interfaces;
type
  IShape = interface
    function Area: Real; const;
    function Name: String; const;
  end;
  IScalable = interface
    procedure Scale(f: Real);
  end;
  ISolid = interface(IShape)
    function Volume(h: Real): Real; const;
  end;
  TCircle = class(IShape, IScalable)
    R: Real;
    constructor Create(AR: Real);
    function Area: Real; const;
    function Name: String; const; virtual;
    procedure Scale(f: Real);
    destructor Destroy; override;
  end;
  TRing = class(TCircle)
    function Name: String; const; override;
  end;
  TSquare = class(TObject, ISolid, IScalable)
    S: Real;
    constructor Create(AS: Real);
    function Area: Real; const;
    function Name: String; const;
    function Volume(h: Real): Real; const;
    procedure Scale(f: Real);
  end;
constructor TCircle.Create(AR: Real); begin R := AR; end;
function TCircle.Area: Real; const; begin Result := 3 * R * R; end;
function TCircle.Name: String; const; begin Result := 'circle'; end;
procedure TCircle.Scale(f: Real); begin R := R * f; end;
destructor TCircle.Destroy; begin WriteLn('free ', Name); end;
function TRing.Name: String; const; begin Result := 'ring'; end;
constructor TSquare.Create(AS: Real); begin S := AS; end;
function TSquare.Area: Real; const; begin Result := S * S; end;
function TSquare.Name: String; const; begin Result := 'square'; end;
function TSquare.Volume(h: Real): Real; const; begin Result := S * S * h; end;
procedure TSquare.Scale(f: Real); begin S := S * f; end;

procedure Show(s: IShape const);
begin
  WriteLn(s.Name, ' ', s.Area:0:1, ' (', s.ClassName, ')');
end;

procedure Grow(var g: IScalable); begin g.Scale(2); end;

function Total<T: IShape>(a: array of optional T): Real;
begin
  Result := 0;
  for val x in a do Result := Result + x.Area;
end;

val shapes: array of optional IShape := default;
begin
  vao c := TCircle.Create(1);
  vao q := TSquare.Create(2);
  begin
    Show(c);
    Show(q);
    var g: IScalable := c;
    begin
      g.Scale(3);
    end;
    Show(c);
    var sol: ISolid := q;
    begin
      WriteLn(sol.Volume(10):0:1);
      Show(sol);
    end;
    WriteLn(c is IScalable, ' ', c is ISolid);
  end;
  SetLength(shapes, 3);
  shapes[0] := TCircle.Create(1);
  shapes[1] := TRing.Create(2);
  shapes[2] := TSquare.Create(3);
  for val s in shapes do Show(s);
  for val s in shapes do
    if s is ISolid then WriteLn(s.Name, ' is solid')
    else if s is TCircle then WriteLn(s.Name, ' is a circle');
  WriteLn(Total(shapes):0:1);
  vao one: IShape := TRing.Create(5);
  begin
    Show(one);
  end;
  WriteLn('end');
end.
