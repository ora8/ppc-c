{* EXPECT: borrowed reference 'm' is used here, but 'a' was used at line 3 while 'm' held a changeable borrow of it (borrow it as 'const' to share it) *}
program T; type TK = class Value: Integer; Child: optional TK; procedure Increment; function Info: Integer; const; end; procedure TK.Increment; begin Inc(Value); end; function TK.Info: Integer; const; begin Result := Value; end;
procedure P; vao a := TK.Create; var m: TK := a; begin  WriteLn(a.Info); m.Increment; end;
begin end.
