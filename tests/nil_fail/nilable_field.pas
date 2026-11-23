{ EXPECT: 'x.Next' may be nil (its type is nilable); copy it into a local variable and check it for nil first }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end;
procedure Show(x: TFoo); begin WriteLn(x.Next.V); end;
begin end.
