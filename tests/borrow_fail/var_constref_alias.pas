{ EXPECT: cannot borrow 'x' as mutable (var) because it is also borrowed in the same call of 'Q' }
program T;
procedure Q(var A: Integer; B: Integer const); begin A := B; end;
val x: Integer := default;
begin x := 1; Q(x, x); end.
