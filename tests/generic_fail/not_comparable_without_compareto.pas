{* EXPECT: "TP" does not satisfy 'T: Comparable' *}
program T; import std; type TP = record X: Integer; end; val a: array of TP := ((X: 2), (X: 1)); begin Sort(a); end.
