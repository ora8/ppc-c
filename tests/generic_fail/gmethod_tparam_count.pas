{* EXPECT: "TA.Id" has 1 type parameter(s) in its declaration *}
program T;
type TA = class function Id<T>(x: T): T; end; function TA.Id<T, U>(x: T): T; begin Result := x; end;
begin end.
