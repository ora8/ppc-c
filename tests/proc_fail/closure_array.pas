{* EXPECT: an array element cannot be *}
program T;
procedure P; val a: array of reference to procedure := default; begin end;
begin end.
