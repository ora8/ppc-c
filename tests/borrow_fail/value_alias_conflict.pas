{* EXPECT: borrowed reference 'r' is used here, but 'n' was changed at line 4 while 'r' was borrowing it *}
program T;
val n := 5; var r: Integer := n;
begin r := r + 1; n := 3; WriteLn(r); end.
