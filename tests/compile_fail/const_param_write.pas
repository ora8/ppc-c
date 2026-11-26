{* EXPECT: assignment: cannot modify constant/loop variable "x" *}
program T;
procedure P(x: Integer const); begin x := 1; end;
begin P(1); end.
