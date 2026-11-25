{ EXPECT: set operations need two sets of the same type }
program T; type TColor = (Red, Green, Blue); TColors = set of TColor; TD = set of 0..9; val s: TColors := [Red]; d: TD := [];
begin WriteLn(s + d); end.
