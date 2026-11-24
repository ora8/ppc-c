{ EXPECT: "TBox" is generic: give its type arguments, e.g. 'TBox<Integer>' }
program T;
type TBox<T> = class Value: T; function Twice: T; end; function TBox<T>.Twice: T; begin Result := Value + Value; end;
vao b: optional TBox := none;
begin end.
