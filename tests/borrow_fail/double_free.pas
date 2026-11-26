{* EXPECT: 'a' is freed twice (double free) *}
program T; type TFoo = class V: Integer; end;
vao a: TFoo := TFoo.Create;
begin  a.Free; a.Free; end.
