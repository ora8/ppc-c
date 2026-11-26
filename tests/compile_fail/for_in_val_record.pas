{* EXPECT: cannot modify a part of 'p', which is constant *}
program T; type TP = record X: Integer; end; val a: array[1..2] of TP := default;
begin for val p in a do p.X := 1; end.
