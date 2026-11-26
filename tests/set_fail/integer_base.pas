{* EXPECT: invalid array index or set element type "Integer" *}
program T; type TColor = (Red, Green, Blue); TColors = set of TColor; TD = set of 0..9; val s: TColors := [Red]; d: TD := [];
type TBig = set of Integer; begin end.
