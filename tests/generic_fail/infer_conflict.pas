{ EXPECT: cannot infer 'T' of "Max": it is "Integer" from one argument and "Char" from another; write 'Max<...>(...)' }
program T;
function Max<T>(a, b: T): T; begin if a > b then Result := a else Result := b; end;
begin WriteLn(Max(3, 'x')); end.
