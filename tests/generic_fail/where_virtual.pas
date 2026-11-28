{* EXPECT: a method with 'where' cannot be virtual or abstract *}
program T; type TBox<T> = class X: T; procedure P; virtual; where T: Comparable; end; procedure TBox<T>.P; begin end; begin end.
