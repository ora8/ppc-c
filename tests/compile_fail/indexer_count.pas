{ EXPECT: the indexer "P" needs 1 index }
program T; type TA = class function G(i: Integer): Integer; property P[i: Integer]: Integer read G; end; function TA.G(i: Integer): Integer; begin Result := i; end;
begin vao a := TA.Create; begin WriteLn(a.P[1, 2]); end; end.
