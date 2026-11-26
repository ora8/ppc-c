{* EXPECT: "TA.M(Char)" is not declared with these parameters *}
program T;
type TA = class procedure M(i: Integer); overload; procedure M(s: String); overload; end; procedure TA.M(i: Integer); begin end; procedure TA.M(s: String); begin end; procedure TA.M(c: Char); begin end;
begin end.
