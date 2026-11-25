{ EXPECT: the field "N" of "TA" is private }
program T;
type TK = class end;
type TA = class private class var N: Integer; end; begin WriteLn(TA.N); end.
