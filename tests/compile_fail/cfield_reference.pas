{* EXPECT: a class field cannot be a 'var' reference *}
program T;
type TK = class end;
type TA = class class var K: var optional TK; end; begin end.
