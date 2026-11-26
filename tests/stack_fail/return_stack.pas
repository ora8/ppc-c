{* EXPECT: 'Make' returns a reference to local variable 's' *}
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Produce: TFoo; begin Result := TFoo.Create(1); end;
function Make: var TFoo; val s := TFoo.Create(1); begin Result := s; end;
begin end.
