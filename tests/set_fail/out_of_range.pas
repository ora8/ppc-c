{* EXPECT: 12 is not in the range 0..9 *}
program T; type TColor = (Red, Green, Blue); TColors = set of TColor; TD = set of 0..9; val s: TColors := [Red]; d: TD := [];
begin d := [12]; end.
