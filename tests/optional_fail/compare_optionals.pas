{ EXPECT: optional values can only be compared with none }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(p, q: optional TFoo); begin if p = q then WriteLn(1); end;
begin end.
