module Shapes;
import Units;     {* from the dependency 'units' *}
interface
type
  TSquare = record
    Side: Real;      {* in inches *}
  end;
function Square(s: Real): TSquare;
function AreaCm2(q: TSquare): Real;
implementation
function Square(s: Real): TSquare;
begin
  Result.Side := s;
end;
function AreaCm2(q: TSquare): Real;
begin
  Result := ToCm(q.Side) * ToCm(q.Side);
end;
end.
