{ EXPECT: generic types must be declared at program level }
program T;
procedure P; type TL<T> = record X: T; end; begin end;
begin end.
