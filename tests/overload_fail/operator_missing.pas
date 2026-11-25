{ EXPECT: "TV" has no operator for this ('class operator Multiply(...)') }
program T;
type TV = record X: Integer; class operator Add(a, b: TV): TV; end; class operator TV.Add(a, b: TV): TV; begin Result.X := a.X + b.X; end; procedure P; val a: TV := (X: 1); begin WriteLn((a * a).X); end;
begin end.
