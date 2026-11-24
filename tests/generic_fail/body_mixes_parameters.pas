{ EXPECT: operator not applicable to "A" and "B" (a type parameter only combines with itself) (in F<A, B>) }
program T;
function F<A, B: Numeric>(a: A; b: B): A; begin Result := a + b; end;
begin end.
