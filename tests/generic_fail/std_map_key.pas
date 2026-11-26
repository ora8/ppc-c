{* EXPECT: "TK" does not satisfy 'K: Comparable' (required by TMap<K, V>) *}
program T;
import ppc_std;
type TK = class end;
vao m := TMap<TK, Integer>.Create;
begin end.
