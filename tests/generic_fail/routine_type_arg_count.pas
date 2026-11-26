{* EXPECT: "Max" has 1 type parameter, but 2 type arguments are given *}
program T;
function Max<T>(a, b: T): T; begin if a > b then Result := a else Result := b; end;
begin WriteLn(Max<Integer, Real>(1, 2)); end.
