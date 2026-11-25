{ EXPECT: the elements of a set are values }
program T; type TColor = (Red, Green, Blue); TColors = set of TColor; TD = set of 0..9; val s: TColors := [Red]; d: TD := [];
begin for var c in s do WriteLn(c); end.
