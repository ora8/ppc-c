{ EXPECT: 'I' is used, but no object was allocated for it (missing 'I := <value>') }
program T;
var I: Integer;
begin WriteLn(I); end.
