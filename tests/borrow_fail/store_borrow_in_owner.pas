{ EXPECT: cannot store borrowed reference 'x' in owning variable 'k' }
program T; type TFoo = class V: Integer; end; PInt = ^Integer;
procedure P(x: TFoo); heap k: TFoo; begin k := x; end;
begin end.
