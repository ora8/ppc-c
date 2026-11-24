{ EXPECT: incompatible types in comparison: "TP" and "TP" (in Max<TP>) }
program T;
type TP = record X: Integer; end; function Max<T>(a, b: T): T; begin if a > b then Result := a else Result := b; end;
val a: TP := default; b: TP := default;
begin a := Max(a, b); end.
