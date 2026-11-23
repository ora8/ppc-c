{ EXPECT: "s" is a stack object and cannot be reassigned }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; var s: TFoo; begin s := TFoo.Create(1); end;
begin P; end.
