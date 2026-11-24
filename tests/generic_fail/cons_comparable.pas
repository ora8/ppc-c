{ EXPECT: "TK" does not satisfy 'T: Comparable' (required by Max<T>) }
program T;
type TK = class end; TR = record X: Integer; end; TA = class end; TB = class(TA) end;
function Max<T: Comparable>(a, b: T): T; begin Result := a; end;
vao k := TK.Create;
begin Max(k, k); end.
