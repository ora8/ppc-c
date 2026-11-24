{ EXPECT: optional value of type "optional TFoo" cannot be used as "TFoo" in initializer; unpack it first }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(q: optional TFoo); var k: TFoo := q; begin  end;
begin end.
