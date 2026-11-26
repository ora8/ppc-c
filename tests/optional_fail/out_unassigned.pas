{* EXPECT: out parameter 'x' is not assigned when 'Get' returns *}
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure Get(out x: TFoo); begin end;
begin end.
