{ EXPECT: assignment: "s" is a stack object and cannot be reassigned (it lives in place until the end of its block; use 'vao' for an object that can be replaced) }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Mach: TFoo; begin Result := TFoo.Create(1); end;
procedure P; val s := TFoo.Create(1); begin s := TFoo.Create(2); end;
begin P; end.
