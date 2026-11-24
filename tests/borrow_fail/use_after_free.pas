{ EXPECT: use of 'a' after it was freed }
program T; type TFoo = class V: Integer; end;
vao a: TFoo := TFoo.Create;
begin  a.Free; WriteLn(a.V); end.
