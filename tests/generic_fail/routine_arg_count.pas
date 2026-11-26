{* EXPECT: wrong number of arguments calling "Max" (expected 2, got 1) *}
program T;
function Max<T>(a, b: T): T; begin if a > b then Result := a else Result := b; end;
begin WriteLn(Max(1)); end.
