{ EXPECT: a stack object always exists, it cannot be optional: use 'vao s: optional TFoo := none' for a heap object that may be missing }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Mach: TFoo; begin Result := TFoo.Create(1); end;
procedure P; val s: optional TFoo := none; begin end;
begin P; end.
