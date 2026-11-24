{ EXPECT: stack object 's' is constructed twice }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; val s: TFoo.Create(1); begin s.Create(2); end;
begin P; end.
