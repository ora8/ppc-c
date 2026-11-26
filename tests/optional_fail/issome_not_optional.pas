{* EXPECT: "TFoo" is not optional, 'is some' needs an optional value *}
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(p: TFoo); begin if p is some k then WriteLn(k.V); end;
begin end.
