{ EXPECT: 's' is a stack object; it is destroyed automatically at the end of its block and must not be freed }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Mach: TFoo; begin Result := TFoo.Create(1); end;
procedure P; val s := TFoo.Create(1); begin s.Free; end;
begin P; end.
