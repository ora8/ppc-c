{* EXPECT: operator needs the constraint 'T: Comparable' (in Max<T>) *}
program T;
type TP = record X: Integer; end; function Max<T>(a, b: T): T; begin if a > b then Result := a else Result := b; end;
val a: TP := default; b: TP := default;
begin a := Max(a, b); end.
