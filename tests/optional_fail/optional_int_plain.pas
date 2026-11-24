{ EXPECT: optional value of type "optional Integer" cannot be used as "Integer" }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(p: optional Integer); val i: Integer := default; begin i := p; end;
begin end.
