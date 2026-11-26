{ EXPECT: this value owns objects: store it in a variable before the case expression ('val x := ...') }
program T;
type TK = class V: Integer; end;
function Find: optional TK; begin Result := TK.Create; end;
begin WriteLn(case Find of some k: k.V; none: 0 end); end.
