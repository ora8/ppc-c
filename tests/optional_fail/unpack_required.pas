{ EXPECT: 'p' is optional (optional TFoo): unpack it first, e.g. 'if p is some v then v.V' }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(p: optional TFoo); begin WriteLn(p.V); end;
begin end.
