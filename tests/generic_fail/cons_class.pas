{* EXPECT: "Integer" does not satisfy 'T: class' (required by TBox<T>) *}
program T;
type TK = class end; TR = record X: Integer; end; TA = class end; TB = class(TA) end;
type TBox<T: class> = class W: optional T; end;
vao b := TBox<Integer>.Create;
begin end.
