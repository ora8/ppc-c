{ EXPECT: "N" is a class field, not a method }
program T;
type TK = class end;
type TA = class class var N: Integer; end; begin TA.N(3); end.
