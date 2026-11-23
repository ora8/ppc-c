{ EXPECT: possibly nil value 'q' cannot be stored in non-nilable 'k' }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end;
procedure P(q: nilable TFoo); var k: borrowed TFoo; begin k := q; end;
begin end.
