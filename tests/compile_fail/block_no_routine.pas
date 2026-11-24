{ EXPECT: only const, val and var sections are allowed before a nested begin ... end }
program T;
begin const C = 1; procedure P; begin end; begin end; end.
