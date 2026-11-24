{ EXPECT: only objects can be constructed in a declaration; for a value write 'i: Integer = ...' }
program T;
val i: Integer.Create(5);
begin end.
