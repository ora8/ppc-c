{ EXPECT: cannot assign nil to non-nilable parameter 'x' of 'Use' }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end; PInt = ^Integer;
procedure Use(x: TFoo); begin WriteLn(x.V); end;
begin Use(nil); end.
