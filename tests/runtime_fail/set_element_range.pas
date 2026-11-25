{ EXPECT: set element out of range }
program T; type TColor = (Red, Green, Blue); TColors = set of TColor; TD = set of 0..9; val s: TColors := [Red]; d: TD := [];
val n := 12; begin Include(d, n); end.
