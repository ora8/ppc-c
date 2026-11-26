{* EXPECT: reference 'r': cannot modify constant/loop variable "n" *}
program T;
val n: Integer const := 5; var r: Integer := n;
begin end.
