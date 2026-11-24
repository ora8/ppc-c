{ EXPECT: "TBox" is generic: give its type arguments, e.g. 'TBox<Integer>' }
program T;
type TBox<T> = class Wert: T; function Doppelt: T; end; function TBox<T>.Doppelt: T; begin Result := Wert + Wert; end;
vao b: optional TBox := none;
begin end.
