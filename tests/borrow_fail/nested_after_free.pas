{ EXPECT: the call of 'Show' uses 'k' after it was freed }
program T; type TK = class V: Integer; end;
procedure P; vao k := TK.Create; procedure Show; begin WriteLn(k.V); end; begin k.Free; Show; end;
begin end.
