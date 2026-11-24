{ EXPECT: operator needs the constraint 'T: Numeric' (in Sum<T>) }
program T;
function Sum<T>(a, b: T): T; begin Result := a + b; end;
begin end.
