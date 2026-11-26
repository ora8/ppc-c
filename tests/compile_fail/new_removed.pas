{* EXPECT: New is not supported (there are no pointers) *}
program T;
val i: Integer := default;
begin New(i); end.
