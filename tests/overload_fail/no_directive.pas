{* EXPECT: duplicate routine "P" (to give several routines the same name, mark each of them with 'overload') *}
program T;
procedure P(i: Integer); begin end; procedure P(s: String); begin end;
begin end.
