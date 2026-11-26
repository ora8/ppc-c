{* EXPECT: unknown constraint 'Sortable': use class, Copy, Clone, Equatable, Comparable, Numeric, Hashable or the name of a class *}
program T;
type TBox<T: Sortable> = record W: T; end;
begin end.
