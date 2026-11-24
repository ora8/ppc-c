{ EXPECT: "TR" does not satisfy 'T: Equatable' (required by Gleich<T>) }
program T;
type TK = class end; TR = record X: Integer; end; TA = class end; TB = class(TA) end;
function Gleich<T: Equatable>(a, b: T): Boolean; begin Result := True; end;
val r: TR := default;
begin WriteLn(Gleich(r, r)); end.
