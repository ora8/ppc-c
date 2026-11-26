{* EXPECT: declare the loop variable: 'for val x in ...' *}
program T;
val a: array[1..3] of Integer := (1, 2, 3);
begin
  for x in a do WriteLn(x);
end.
