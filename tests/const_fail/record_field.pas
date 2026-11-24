{ EXPECT: cannot modify a part of 'Self', which is constant }
program T; type TV = record X: Real; const procedure P; end;
const procedure TV.P; begin X := 1; end;
begin end.
