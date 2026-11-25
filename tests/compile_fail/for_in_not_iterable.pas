{ EXPECT: cannot be used in 'for ... in' }
program T;
val i := 5;
begin
  for val x in i do WriteLn(x);
end.
