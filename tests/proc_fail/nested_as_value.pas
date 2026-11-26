{* EXPECT: uses variables of the routine around it (a closure) and cannot be stored as "function(): Integer" *}
program T;
type TF = function: Integer;
procedure P; function Inner: Integer; begin Result := 1; end; val f: TF := Inner; begin end;
begin P; end.
