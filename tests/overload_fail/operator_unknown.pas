{* EXPECT: unknown operator "Plus" *}
program T;
type TV = record X: Integer; class operator Plus(a, b: TV): TV; end;
begin end.
