{ EXPECT: "T" does not satisfy 'T: Comparable' (required by Max<T>) (in Groesser<T>) }
program T;
function Max<T: Comparable>(a, b: T): T; begin if a > b then Result := a else Result := b; end;
function Groesser<T>(x, y: T): T; begin Result := Max(x, y); end;
begin end.
