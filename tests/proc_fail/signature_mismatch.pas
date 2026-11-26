{* EXPECT: incompatible types in initializer: got "function(String): Integer" expected "function(Integer): Integer" *}
program T;
type TF = function(x: Integer): Integer;
function G(x: String): Integer; begin Result := 0; end;
val f: TF := G;
begin end.
