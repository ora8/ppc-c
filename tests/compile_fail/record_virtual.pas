{* EXPECT: record methods cannot be virtual, abstract or override *}
program T;
type R = record X: Integer; procedure P; virtual; end;
procedure R.P; begin end;
begin end.
