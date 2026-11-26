module Geometry;
{* a module of our own: included by modules.pas with 'import Geometry;' *}
import ppc_std;

interface

const
  Pi2 = 6.283185307179586;

type
  TPoint = record
    X, Y: Real;
    function Distance(b: TPoint): Real; const;
  end;
  TFigure = class
    Name: String;
    constructor Create(AName: String const);
    function Area: Real; virtual;
  end;
  TCircle = class(TFigure)
    R: Real;
    constructor Create(AR: Real);
    function Area: Real; override;
  end;

function TotalArea(l: TList<TFigure>): Real;

implementation

function Square(x: Real): Real;                    {* private: not visible to importers *}
begin Result := x * x; end;

function TPoint.Distance(b: TPoint): Real; const;
begin Result := Sqrt(Square(X - b.X) + Square(Y - b.Y)); end;

constructor TFigure.Create(AName: String const); begin inherited Create; Name := AName; end;
function TFigure.Area: Real; begin Result := 0; end;
constructor TCircle.Create(AR: Real); begin inherited Create('Circle'); R := AR; end;
function TCircle.Area: Real; begin Result := Pi2 / 2 * R * R; end;

function TotalArea(l: TList<TFigure>): Real;
begin
  Result := 0;
  for val i := 0 to l.Count - 1 do
    if l.Get(i) is some f then Result := Result + f.Area;
end;

end.
