{* EXPECT: "TK" does not satisfy 'K: Comparable' (required by TMap<K, V>) *}
program T;
type TMap<K: Comparable; V> = class end;
type TK = class end;
vao m := TMap<TK, Integer>.Create;
begin end.
