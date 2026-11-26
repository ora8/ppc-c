{* EXPECT: "TP.Equals" is used by '=', '<' or Hash: declare it 'const' *}
program T; type TP = record X: Integer; function Equals(o: TP): Boolean; end; function TP.Equals(o: TP): Boolean; begin Result := X = o.X; end; val a: TP := (X: 1); val b: TP := (X: 1); begin WriteLn(a = b); end.
