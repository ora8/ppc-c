{* EXPECT: class field of type "TK" must be declared 'optional TK' *}
program T;
type TK = class end;
type TA = class class var K: TK; end; begin end.
