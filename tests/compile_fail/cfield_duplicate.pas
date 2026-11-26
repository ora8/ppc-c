{* EXPECT: duplicate field "N" *}
program T;
type TK = class end;
type TA = class class var N: Integer; N: Integer; end; begin end.
