{* EXPECT: the enumerator "TE" needs 'function MoveNext: Boolean' *}
program T; type TE = record function Current: Integer; end; TNo = record function GetEnumerator: TE; end; function TE.Current: Integer; begin Result := 0; end; function TNo.GetEnumerator: TE; begin end; val n: TNo := default;
begin for val x in n do; end.
