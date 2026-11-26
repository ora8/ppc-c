{* EXPECT: borrowed reference 'm1' is used here, but 'a' was borrowed again as changeable at line 3 while 'm1' was borrowing it *}
program T; type TK = class Value: Integer; Child: optional TK; procedure Increment; function Info: Integer; const; end; procedure TK.Increment; begin Inc(Value); end; function TK.Info: Integer; const; begin Result := Value; end;
procedure P; vao a := TK.Create; var m1: TK := a; m2: TK := a; begin   m2.Increment; m1.Increment; end;
begin end.
