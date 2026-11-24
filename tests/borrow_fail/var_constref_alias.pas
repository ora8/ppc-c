{ EXPECT: cannot borrow 'x' as mutable (var) because it is also borrowed in the same call of 'Q' }
program T;
procedure Q(var A: Integer; constref B: Integer); begin A := B; end;
val x: Integer;
begin x := 1; Q(x, x); end.
