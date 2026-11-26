{* EXPECT: 'is some' is only allowed in if/while conditions (alone or combined with 'and') *}
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(p: optional TFoo; c: Boolean); begin if (p is some k) or c then WriteLn(1); end;
begin end.
