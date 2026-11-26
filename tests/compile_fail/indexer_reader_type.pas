{* EXPECT: the reader "G" of the indexer "P" must be a function with the index parameters and the result type "Integer" *}
program T; type TA = class function G(i: Integer): String; property P[i: Integer]: Integer read G; end; function TA.G(i: Integer): String; begin Result := ''; end;
begin vao a := TA.Create; begin WriteLn(a.P[1]); end; end.
