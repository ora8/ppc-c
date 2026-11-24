{ EXPECT: cannot infer type parameter 'T' of "Neu" from the arguments: write 'Neu<...>(...)' }
program T;
function Neu<T>: Integer; begin Result := 0; end;
begin WriteLn(Neu); end.
