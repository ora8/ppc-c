{ EXPECT: identifier not found "k" }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(p: optional TFoo); begin if p is some k then WriteLn(k.V) else WriteLn(k.V); end;
begin end.
