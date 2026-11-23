{ EXPECT: 'p' may be nil here; check it with 'if p <> nil then' before using it }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end;
function Find: borrowed nilable TFoo; begin Result := nil; end;
var p: borrowed nilable TFoo;
begin p := Find; WriteLn(p.V); end.
