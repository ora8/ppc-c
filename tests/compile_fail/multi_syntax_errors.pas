{* EXPECT: 6 syntax error(s), compilation aborted *}
program E1;
procedure A;
begin
  WriteLn('a' 1);
  WriteLn('b');
  x := := 3;
end;

procedure B(x: Integer;
begin
end;

procedure C;
begin
  if then WriteLn(1);
  WriteLn('c')
  WriteLn('d');
end;

begin
  A;
  := 5;
end.
