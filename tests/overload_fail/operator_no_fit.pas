{ EXPECT: no operator Add fits the operands "TV" and "Integer" }
program T;
type TV = record X: Integer; class operator Add(a, b: TV): TV; end; class operator TV.Add(a, b: TV): TV; begin Result.X := a.X + b.X; end; procedure P; val a: TV := (X: 1); begin WriteLn((a + 1).X); end;
begin end.
