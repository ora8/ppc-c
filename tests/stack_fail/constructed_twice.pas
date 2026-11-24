{ EXPECT: stack object 's' is constructed twice }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; val s: TFoo; begin s.Create(1); s.Create(2); end;
begin P; end.
