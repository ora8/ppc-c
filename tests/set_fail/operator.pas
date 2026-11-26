{* EXPECT: this operator is not defined for sets *}
program T; type TColor = (Red, Green, Blue); TColors = set of TColor; TD = set of 0..9; val s: TColors := [Red]; d: TD := [];
begin WriteLn(s < s); end.
