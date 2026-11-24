{ EXPECT: "TBox" has 1 type parameter, but 2 type arguments are given }
program T;
type TBox<T> = class Wert: T; function Doppelt: T; end; function TBox<T>.Doppelt: T; begin Result := Wert + Wert; end;
vao b := TBox<Integer, String>.Create;
begin end.
