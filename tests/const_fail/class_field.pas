{ EXPECT: cannot modify a part of 'Self', which is constant }
program T; type TK = class F: Integer; const procedure P; end;
const procedure TK.P; begin F := 1; end;
begin end.
