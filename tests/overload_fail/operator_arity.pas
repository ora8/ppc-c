{* EXPECT: the operator Add needs 2 operands *}
program T;
type TV = record X: Integer; class operator Add(a: TV): TV; end;
begin end.
