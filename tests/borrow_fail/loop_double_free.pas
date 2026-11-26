{* EXPECT: 'a' is possibly freed twice (double free) *}
program T; type TFoo = class V: Integer; end;
vao a: TFoo := TFoo.Create; val i: Integer := default;
begin  for i := 1 to 3 do a.Free; end.
