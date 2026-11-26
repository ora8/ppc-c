{* EXPECT: the implementation of "TA.Id" does not match its declaration *}
program T;
type TA = class function Id<T>(x: T): T; end; function TA.Id<T>(x: Integer): T; begin end;
begin end.
