{ EXPECT: borrowed reference 'k' is used after its owner 'h' was freed }
program T; type TFoo = class V: Integer; end;
var h: optional TFoo;
begin h := TFoo.Create; if h is some k then begin h.Free; WriteLn(k.V); end; end.
