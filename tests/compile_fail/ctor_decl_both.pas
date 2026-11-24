{ EXPECT: use either 'x: T.Create(...)' or 'x: T = ...', not both }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
var h: TFoo.Create(1) = TFoo.Create(2);
begin end.
