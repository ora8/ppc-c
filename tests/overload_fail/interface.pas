{* EXPECT: interfaces cannot overload methods *}
program T;
type I = interface procedure M(i: Integer); overload; end;
begin end.
