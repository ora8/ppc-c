{* EXPECT: the generic method "TA.Id" is not implemented *}
program T;
type TA = class function Id<T>(x: T): T; end; procedure P; vao a := TA.Create; begin WriteLn(a.Id(1)); end;
begin end.
