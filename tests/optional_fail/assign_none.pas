{* EXPECT: none can only be stored in an optional type, not in "TFoo" *}
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
vao a: TFoo := TFoo.Create;
begin  a.Free; a := none; end.
