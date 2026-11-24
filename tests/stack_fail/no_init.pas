{ EXPECT: a stack object is created in its declaration: 'val s := TFoo.Create(...)' (or 'vao' for a heap object, 'var' for a reference) }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Mach: TFoo; begin Result := TFoo.Create(1); end;
procedure P; val s: TFoo; begin end;
begin P; end.
