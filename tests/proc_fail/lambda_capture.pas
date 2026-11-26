{* EXPECT: uses variables of the routine around it (a closure) and cannot be stored as "function(): Integer" *}
program T;
type TF = function: Integer;
procedure P; val n := 5; val f: TF := function: Integer begin Result := n; end; begin end;
begin P; end.
