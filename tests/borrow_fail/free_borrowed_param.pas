{ EXPECT: cannot free borrowed reference 'x' }
program T; type TFoo = class V: Integer; end;
procedure Kill(x: TFoo); begin x.Free; end;
vao a: TFoo := TFoo.Create;
begin  Kill(a); a.Free; end.
