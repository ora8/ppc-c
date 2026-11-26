{* EXPECT: there is no method in an ancestor class to be overridden *}
program T;
type A = class procedure P; override; end;
procedure A.P; begin end;
begin end.
