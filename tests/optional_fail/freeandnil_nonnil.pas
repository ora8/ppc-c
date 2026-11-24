{ EXPECT: FreeAndNil would leave non-nilable 'a' nil }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
vao a: TFoo := TFoo.Create;
begin  FreeAndNil(a); end.
