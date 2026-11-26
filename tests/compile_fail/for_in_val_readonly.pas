{* EXPECT: cannot modify constant/loop variable "x" *}
program T;
val a: array[1..3] of Integer := (1, 2, 3);
begin
  for val x in a do x := 5;
end.
