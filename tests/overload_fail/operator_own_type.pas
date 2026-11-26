{* EXPECT: an operator of "TV" needs an operand of type "TV" *}
program T;
type TV = record X: Integer; class operator Add(a, b: Integer): Integer; end;
begin end.
