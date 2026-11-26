{* EXPECT: "TP" has 2 type parameters *}
program T;
type TP<A, B> = record X: A; procedure Z; end; procedure TP<A>.Z; begin end;
begin end.
