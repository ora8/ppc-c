{ EXPECT: cannot move out of 'a[...]' }
program T;
function First<T>(a: array of optional T): optional T; begin Result := a[0]; end;
begin end.
