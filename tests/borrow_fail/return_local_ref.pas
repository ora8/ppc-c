{ EXPECT: returns a reference to local variable 'n' }
program T; type TFoo = class V: Integer; end; PInt = ^Integer;
function F: borrowed PInt; var n: Integer; begin Result := @n; end;
begin WriteLn(F^); end.
