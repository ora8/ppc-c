{ EXPECT: a lambda cannot use the local variable 'n' of the routine around it (pass it as a parameter) }
program T;
type TF = function: Integer;
procedure P; val n := 5; val f: TF := function: Integer begin Result := n; end; begin end;
begin P; end.
