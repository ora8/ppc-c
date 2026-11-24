{ EXPECT: 'is some' is only allowed in if/while conditions }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(p: optional TFoo); val b: Boolean := default; begin b := p is some k; end;
begin end.
