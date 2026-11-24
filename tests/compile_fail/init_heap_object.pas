{ EXPECT: 'a' must be initialized: 'vao a := TFoo.Create(...)' (or 'vao a: optional TFoo := none') }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
vao a: TFoo;
begin a.V := 1; end.
