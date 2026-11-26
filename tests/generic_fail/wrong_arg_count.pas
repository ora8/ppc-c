{* EXPECT: "TBox" has 1 type parameter, but 2 type arguments are given *}
program T;
type TBox<T> = class Value: T; function Twice: T; end; function TBox<T>.Twice: T; begin Result := Value + Value; end;
vao b := TBox<Integer, String>.Create;
begin end.
