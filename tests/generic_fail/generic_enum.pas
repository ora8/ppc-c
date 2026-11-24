{ EXPECT: only records and classes can be generic }
program T;
type TE<T> = (A, B);
begin end.
