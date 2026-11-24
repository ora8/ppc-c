{ EXPECT: const function 'F' cannot change the non-local variable 'G' }
program T; val G: Integer := default;
function F: Integer; const; begin G := 1; Result := G; end;
begin end.
