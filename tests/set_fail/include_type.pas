{ EXPECT: "TColor" is not an element type of "set of 0..9" }
program T; type TColor = (Red, Green, Blue); TColors = set of TColor; TD = set of 0..9; val s: TColors := [Red]; d: TD := [];
begin Include(d, Red); end.
