{ EXPECT: ValueOr is only available for optional values }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(p: optional TFoo; d: TFoo); begin WriteLn(p.ValueOr(d).V); end;
begin end.
