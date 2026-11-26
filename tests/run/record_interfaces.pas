program RecordInterfaces;
{* records implement interfaces: record(IShape); generic routines with T: IShape take records and classes *}
type
  IShape = interface
    function Area: Real; const;
    function Name: String; const;
  end;
  TSquare = record(IShape)
    Side: Real;
    function Area: Real; const;
    function Name: String; const;
  end;
  TCircle = class(TObject, IShape)
    R: Real;
    constructor Create(AR: Real);
    function Area: Real; const;
    function Name: String; const;
  end;
function TSquare.Area: Real; const; begin Result := Side * Side; end;
function TSquare.Name: String; const; begin Result := 'square'; end;
constructor TCircle.Create(AR: Real); begin inherited Create; R := AR; end;
function TCircle.Area: Real; const; begin Result := 3 * R * R; end;
function TCircle.Name: String; const; begin Result := 'circle'; end;

function Describe<T: IShape>(x: T): String;
begin
  Result := x.Name + ' with area ' + FloatToStr(x.Area);
end;

function Total<T: IShape>(a: array of T): Real;
begin
  Result := 0;
  for val i := 0 to High(a) do Result := Result + a[i].Area;
end;

val sq: TSquare := (Side: 3);
val squares: array of TSquare := ((Side: 1), (Side: 2));
vao c := TCircle.Create(1);
begin
  WriteLn(Describe(sq));
  WriteLn(Describe(c));
  WriteLn(Total(squares):0:1);
end.
