{* EXPECT: "TBox<T>.Big" needs 'T: Comparable' *}
program T; type TBox<T> = class A, B: T; function Big: Boolean; where T: Comparable; procedure Show; end; function TBox<T>.Big: Boolean; begin Result := A < B; end; procedure TBox<T>.Show; begin if Big then WriteLn(1); end; begin end.
