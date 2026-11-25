{ EXPECT: the indexer "P" is read-only (it has no 'write') }
program T; type TA = class function G(i: Integer): Integer; property P[i: Integer]: Integer read G; end; function TA.G(i: Integer): Integer; begin Result := i; end;
begin vao a := TA.Create; begin a.P[1] := 3; end; end.
