{ EXPECT: a lambda cannot use 'V' of the object around it (pass the object as a parameter) }
program T;
type TF = function: Integer; TK = class V: Integer; function Get: TF; end;
function TK.Get: TF; begin Result := function: Integer begin Result := V; end; end;
begin end.
