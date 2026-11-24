{ EXPECT: operator needs the constraint 'T: Numeric' (in TBox<T>.Twice) }
program T;
type TK = class end; type TBox<T> = class Value: T; function Twice: T; end; function TBox<T>.Twice: T; begin Result := Value + Value; end;
vao b := TBox<TK>.Create;
begin end.
