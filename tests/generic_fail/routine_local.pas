{* EXPECT: generic routines must be declared at program level *}
program T;
procedure P; function Max<T>(a: T): T; begin Result := a; end; begin end;
begin end.
