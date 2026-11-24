{ EXPECT: operator not applicable to "TK" and "TK" (in TBox<TK>.Doppelt) }
program T;
type TK = class end; type TBox<T> = class Wert: T; function Doppelt: T; end; function TBox<T>.Doppelt: T; begin Result := Wert + Wert; end;
vao b := TBox<TK>.Create;
begin end.
