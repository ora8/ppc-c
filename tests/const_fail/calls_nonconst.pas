{ EXPECT: const function 'F' can only call const routines, but 'P' is not const }
program T; val G: Integer; procedure P; begin G := 2; end;
const function F: Integer; begin P; Result := 1; end;
begin end.
