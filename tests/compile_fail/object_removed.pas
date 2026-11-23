{ EXPECT: 'object' types are not supported: use 'class' (class variables in 'var' are stack objects, in 'heap' heap objects) }
program T;
type A = object X: Integer; end;
begin end.
