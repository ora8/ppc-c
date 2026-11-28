{* EXPECT: operator needs the constraint *}
program T; type TBox<T> = class A, B: T; function Big: Boolean; where T: Equatable; end; function TBox<T>.Big: Boolean; begin Result := A < B; end; begin end.
