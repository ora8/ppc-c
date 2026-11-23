{ EXPECT: reference to local variable 'n' escapes into 'G' }
program T; type TFoo = class V: Integer; end; PInt = ^Integer;
var G: borrowed PInt;
procedure P; var n: Integer; begin n := 5; G := @n; end;
begin P; end.
