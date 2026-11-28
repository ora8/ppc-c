{* EXPECT: 'where' takes the built-in constraints *}
program T; type TBase = class end; TBox<T> = class X: T; procedure P; where T: TBase; end; procedure TBox<T>.P; begin end; begin end.
