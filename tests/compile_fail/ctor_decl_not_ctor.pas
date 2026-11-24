{ EXPECT: "TFoo.Free" is neither a constructor nor a class method }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
vao s := TFoo.Free;
begin end.
