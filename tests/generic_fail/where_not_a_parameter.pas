{* EXPECT: 'where U': 'U' is not a type parameter of this generic type *}
program T; type TBox<T> = class X: T; procedure P; where U: Comparable; end; procedure TBox<T>.P; begin end; begin end.
