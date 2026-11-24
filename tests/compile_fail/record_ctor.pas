{ EXPECT: records cannot have constructors or destructors }
program T;
type R = record X: Integer; constructor Create; end;
begin end.
