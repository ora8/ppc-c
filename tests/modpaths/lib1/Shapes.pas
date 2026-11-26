module Shapes;
import Helper;      {* found next to this module, not next to the program *}
interface
type
  TSquare = record
    Side: Real;
  end;
function Square(s: Real): TSquare;
function Area(q: TSquare): Real;
implementation
function Square(s: Real): TSquare; begin Result.Side := s; end;
function Area(q: TSquare): Real; begin Result := Sqr2(q.Side); end;
end.
