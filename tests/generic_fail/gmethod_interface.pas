{ EXPECT: interfaces cannot have generic methods }
program T;
type I = interface function Id<T>(x: T): T; end;
begin end.
