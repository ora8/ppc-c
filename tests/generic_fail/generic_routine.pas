{ EXPECT: generic functions and procedures ('Max<T>') are not supported yet: only records and classes can be generic }
program T;
function Max<T>(a, b: T): T; begin Result := a; end;
begin end.
