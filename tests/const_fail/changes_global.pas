{ EXPECT: const function 'F' cannot change the non-local variable 'G' }
program T; val G: Integer;
const function F: Integer; begin G := 1; Result := G; end;
begin end.
