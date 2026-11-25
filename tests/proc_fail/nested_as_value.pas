{ EXPECT: only routines declared at program or module level can be used as values ('Inner' is nested) }
program T;
type TF = function: Integer;
procedure P; function Inner: Integer; begin Result := 1; end; val f: TF := Inner; begin end;
begin P; end.
