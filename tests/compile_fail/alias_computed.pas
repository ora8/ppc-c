{* EXPECT: 'r' must refer to a variable, not to a computed value *}
program T;
val n := 5; var r: Integer := n + 1;
begin end.
