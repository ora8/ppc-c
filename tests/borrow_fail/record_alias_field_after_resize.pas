{* EXPECT: borrowed reference 'r' is used after its owner 'a' was resized by SetLength *}
program T; type TP = record X: Integer; end; val a: array of TP := default;
begin SetLength(a, 1); var r := a[0]; begin SetLength(a, 0); WriteLn(r.X); end; end.
