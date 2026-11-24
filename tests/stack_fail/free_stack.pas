{ EXPECT: 's' is a stack object; it is destroyed automatically at the end of its block and must not be freed }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; val s: TFoo; begin s.Create(1); s.Free; end;
begin P; end.
