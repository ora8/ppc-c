{ EXPECT: unknown constraint 'Sortierbar': use class, Copy, Clone, Equatable, Comparable, Numeric or the name of a class }
program T;
type TBox<T: Sortierbar> = record W: T; end;
begin end.
