{ EXPECT: cannot modify a part of 'Self', which is constant }
program T; type TK = class F: Integer; procedure P; const; end;
procedure TK.P; const; begin F := 1; end;
begin end.
