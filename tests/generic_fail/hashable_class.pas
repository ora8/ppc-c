{ EXPECT: "TK" does not satisfy 'K: Hashable' }
program T; import ppc_std; type TK = class end;
begin vao m := THashMap<TK, Integer>.Create; begin end; end.
