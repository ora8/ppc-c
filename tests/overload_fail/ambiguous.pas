{* EXPECT: the call of "P" with (Integer, Integer) is ambiguous *}
program T;
procedure P(a: Int64; b: Real); overload; begin end; procedure P(a: Real; b: Int64); overload; begin end; procedure Q; val i: Integer := 1; begin P(i, i); end;
begin end.
