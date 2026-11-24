{ EXPECT: 'Name' is not known for type parameter "T": add a constraint that provides it (e.g. 'T: TBasis' for a class, 'T: Clone' for Clone) (in N<T>) }
program T;
function N<T>(x: T): String; begin Result := x.Name; end;
begin end.
