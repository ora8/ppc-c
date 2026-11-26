{* EXPECT: "TK" cannot be hashed *}
program T; type TK = class end;
begin vao k := TK.Create; begin WriteLn(Hash(k)); end; end.
