{ EXPECT: borrowed reference 'l' is used here, but 'a' was borrowed again as changeable at line 3 while 'l' was borrowing it }
program T; type TK = class Value: Integer; Child: optional TK; procedure Increment; function Info: Integer; const; end; procedure TK.Increment; begin Inc(Value); end; function TK.Info: Integer; const; begin Result := Value; end;
procedure P; vao a := TK.Create; var l: TK const := a; begin var m: TK := a; begin m.Increment; end; WriteLn(l.Info); end;
begin end.
