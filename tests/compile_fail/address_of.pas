{ EXPECT: the address operator '@' is not supported }
program T;
val i: Integer := default;
begin WriteLn(Integer(@i)); end.
