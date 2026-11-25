{ EXPECT: GetEnumerator of "TNo" must return a record or an object }
program T; type TNo = record function GetEnumerator: Integer; end; function TNo.GetEnumerator: Integer; begin Result := 0; end; val n: TNo := default;
begin for val x in n do; end.
