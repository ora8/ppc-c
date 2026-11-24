{ EXPECT: optional value of type "optional TFoo" cannot be used as "TFoo" in assignment; unpack it first }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(q: optional TFoo); val k: borrowed TFoo; begin k := q; end;
begin end.
