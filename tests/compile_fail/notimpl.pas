{* EXPECT: method "A.Q" is declared but not implemented *}
program T;
type A = class procedure Q; end;
begin end.
