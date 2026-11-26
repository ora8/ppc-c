{* EXPECT: a 'reference to' parameter can only be a value parameter *}
program T;
procedure P(var f: reference to procedure); begin end;
begin end.
