{ EXPECT: 's' is a stack object and stays in its block: it cannot be given to the 'vao' parameter 'x' of 'Nimm' (pass a copy: 's.Clone') }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Mach: TFoo; begin Result := TFoo.Create(1); end;
procedure Nimm(vao x: TFoo); begin end;
procedure P; val s := TFoo.Create(1); begin Nimm(s); end;
begin P; end.
