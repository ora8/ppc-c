{ EXPECT: "Free" is not a constructor of "TFoo" }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
val s: TFoo.Free;
begin end.
