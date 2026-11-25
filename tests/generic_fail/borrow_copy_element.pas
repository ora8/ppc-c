{ EXPECT: cannot move out of 'Items[...]' (it is owned by its container); use Take(Items[...]) }
program T;
type TBox<T> = class Items: array of optional T; procedure Dup; end; procedure TBox<T>.Dup; begin SetLength(Items, 2); Items[1] := Items[0]; end;
begin end.
