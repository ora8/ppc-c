{ EXPECT: use of moved value 'x' }
program T;
type TPair<T> = record A, B: optional T; end; function Twice<T>(vao x: T): TPair<T>; begin Result.A := x; Result.B := x; end;
begin end.
