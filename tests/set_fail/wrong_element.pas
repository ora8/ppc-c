{ EXPECT: "Integer" cannot be an element of "set of TColor" }
program T; type TColor = (Red, Green, Blue); TColors = set of TColor; TD = set of 0..9; val s: TColors := [Red]; d: TD := [];
begin s := [1]; end.
