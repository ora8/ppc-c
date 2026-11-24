{ EXPECT: 'object' types are not supported: use 'class' (class variables in 'val' are stack objects, in 'var' heap objects) }
program T;
type A = object X: Integer; end;
begin end.
