{ EXPECT: "TK" does not satisfy 'T: Clone' (required by Doppel<T>) }
program T;
type TK = class end; TR = record X: Integer; end; TA = class end; TB = class(TA) end;
function Doppel<T: Clone>(x: T): Integer; begin Result := 1; end;
vao k := TK.Create;
begin WriteLn(Doppel(k)); end.
