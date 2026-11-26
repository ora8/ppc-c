{* EXPECT: borrowed reference 'r' is used after its owner 's' went out of scope at the end of its block (line 5) *}
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Make: TFoo; begin Result := TFoo.Create(1); end;
var r: optional TFoo := none;
begin val s := TFoo.Create(1); begin r := s; end; if r is some x then WriteLn(x.V); end.
