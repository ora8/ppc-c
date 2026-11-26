{* EXPECT: identifier not found "Self" *}
program T;
type TK = class class procedure F; end; class procedure TK.F; begin WriteLn(Self.ClassName); end;
begin end.
