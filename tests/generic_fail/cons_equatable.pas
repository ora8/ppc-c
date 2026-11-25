{ EXPECT: "TR" does not satisfy 'T: Equatable' (required by Equal<T>) }
program T;
type TK = class end; TR = record F: optional function: Integer; end; TA = class end; TB = class(TA) end;
function Equal<T: Equatable>(a, b: T): Boolean; begin Result := True; end;
val r: TR := default;
begin WriteLn(Equal(r, r)); end.
