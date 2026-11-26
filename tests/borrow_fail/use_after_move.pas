{* EXPECT: use of moved value 'a' *}
program T; type TFoo = class V: Integer; end;
vao a: TFoo := TFoo.Create; b: TFoo := a;
begin   a.V := 1; b.Free; end.
