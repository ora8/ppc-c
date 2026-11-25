{ EXPECT: generic methods cannot be virtual }
program T;
type TA = class function Id<T>(x: T): T; virtual; end;
begin end.
