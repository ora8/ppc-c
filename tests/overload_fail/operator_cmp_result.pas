{ EXPECT: the comparison operator Equal must return Boolean }
program T;
type TV = record X: Integer; class operator Equal(a, b: TV): Integer; end;
begin end.
