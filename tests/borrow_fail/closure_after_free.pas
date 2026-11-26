{* EXPECT: the call through a function value (a closure) uses 'k' after it was freed *}
program T; type TK = class V: Integer; end;
procedure P; vao k := TK.Create; val f: reference to procedure := procedure begin WriteLn(k.V); end; begin k.Free; f; end;
begin end.
