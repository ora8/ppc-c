{ EXPECT: borrowed reference 'r' is used after its owner 'a' was resized by SetLength at line 3 }
program T;
procedure P; val a: array of Integer := (1, 2); begin var r: Integer := a[0]; begin SetLength(a, 100); r := 5; end; end;
begin P; end.
