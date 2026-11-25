{ EXPECT: the error of a result must be a value without objects, not "TK" }
program T;
type TK = class end; function G: result Integer of TK; begin end;
begin end.
