{ EXPECT: cannot free borrowed reference 'x' }
program T; type TFoo = class V: Integer; end; PInt = ^Integer;
procedure Kill(x: TFoo); begin x.Free; end;
heap a: TFoo;
begin a := TFoo.Create; Kill(a); a.Free; end.
