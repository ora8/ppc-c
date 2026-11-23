{ EXPECT: only class and pointer types can be declared in a heap section }
program T;
procedure P; heap i: Integer; begin end;
begin P; end.
