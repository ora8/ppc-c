{ EXPECT: "TK" does not satisfy 'T: Copy' (required by TBox<T>) }
program T;
type TK = class end; TR = record X: Integer; end; TA = class end; TB = class(TA) end;
type TBox<T: Copy> = record W: T; end;
val b: TBox<TK> := default;
begin end.
