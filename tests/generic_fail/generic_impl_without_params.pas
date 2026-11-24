{ EXPECT: "TBox" is generic: write 'procedure TBox<...>.Doppelt' }
program T;
type TBox<T> = class Wert: T; function Doppelt: T; end; function TBox<T>.Doppelt: T; begin Result := Wert + Wert; end;
function TBox.Doppelt: Integer; begin Result := 1; end;
begin end.
