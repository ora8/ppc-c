{ EXPECT: assignment: cannot modify constant/loop variable "r" }
program T;
val n := 5; var r: Integer const := n;
begin r := 1; end.
