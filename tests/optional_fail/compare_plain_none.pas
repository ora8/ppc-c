{* EXPECT: "TFoo" is not optional and can never be none *}
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(p: TFoo); begin if p = none then WriteLn(1); end;
begin end.
