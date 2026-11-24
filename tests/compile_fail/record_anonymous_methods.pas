{ EXPECT: records with methods must be declared as a named type }
program T;
val r: record X: Integer; procedure P; end;
begin end.
