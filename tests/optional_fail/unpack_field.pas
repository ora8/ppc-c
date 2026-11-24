{ EXPECT: 'Next' is optional (optional TFoo): unpack it first }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(x: TFoo); begin WriteLn(x.Next.V); end;
begin end.
