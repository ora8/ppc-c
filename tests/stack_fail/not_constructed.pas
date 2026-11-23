{ EXPECT: stack object 's' is used before it was constructed (missing 's.Create(...)') }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; var s: TFoo; begin WriteLn(s.V); end;
begin P; end.
