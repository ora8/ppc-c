{* EXPECT: "TBox" is generic: write 'procedure TBox<...>.Twice' *}
program T;
type TBox<T> = class Value: T; function Twice: T; end; function TBox<T>.Twice: T; begin Result := Value + Value; end;
function TBox.Twice: Integer; begin Result := 1; end;
begin end.
