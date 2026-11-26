{* EXPECT: "Max" is not a generic type *}
program T;
function Max<T>(a, b: T): T; begin if a > b then Result := a else Result := b; end;
val x: Max<Integer> := 0;
begin end.
