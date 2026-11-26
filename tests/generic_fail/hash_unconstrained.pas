{* EXPECT: Hash needs the constraint 'T: Hashable' *}
program T; function H<T>(x: T): Int64; begin Result := Hash(x); end;
begin end.
