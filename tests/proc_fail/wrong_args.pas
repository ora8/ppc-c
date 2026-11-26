{* EXPECT: wrong number of arguments calling "function value" (expected 1, got 2) *}
program T;
type TF = function(x: Integer): Integer;
function G(x: Integer): Integer; begin Result := x; end;
val f: TF := G;
begin WriteLn(f(1, 2)); end.
