{ EXPECT: operator needs the constraint 'T: Numeric' (in Summe<T>) }
program T;
function Summe<T>(a, b: T): T; begin Result := a + b; end;
begin end.
