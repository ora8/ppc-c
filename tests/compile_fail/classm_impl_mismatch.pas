{* EXPECT: "TK.F" is declared as class method: the implementation must match ('class procedure ...') *}
program T;
type TK = class class procedure F; end; procedure TK.F; begin end;
begin end.
