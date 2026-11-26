{* EXPECT: a set can have at most 256 values *}
program T; type TColor = (Red, Green, Blue); TColors = set of TColor; TD = set of 0..9; val s: TColors := [Red]; d: TD := [];
type TBig = set of 0..300; begin end.
