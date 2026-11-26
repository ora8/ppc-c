{* EXPECT: uses variables of the routine around it (a closure) and cannot be stored as "function(): Integer" *}
program T;
type TF = function: Integer; TK = class V: Integer; function Get: TF; end;
function TK.Get: TF; begin Result := function: Integer begin Result := V; end; end;
begin end.
