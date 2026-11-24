{ EXPECT: "TA" does not satisfy 'T: TB' (required by TZoo<T>) }
program T;
type TK = class end; TR = record X: Integer; end; TA = class end; TB = class(TA) end;
type TZoo<T: TB> = class W: optional T; end;
vao z := TZoo<TA>.Create;
begin end.
