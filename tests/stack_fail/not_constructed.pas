{ EXPECT: a stack object is created in its declaration: 'val s := TFoo.Create(...)' (for a reference use 'var', to own a heap object 'vao') }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Mach: TFoo; begin Result := TFoo.Create(1); end;
procedure P; val s := Mach; begin end;
begin P; end.
