{* EXPECT: use of moved value 'x' (ownership was transferred away) (in the generic Twice *}
program T;
procedure Use<T>(vao x: T); begin end; procedure Twice<T>(vao x: T); begin Use(x); Use(x); end;
begin end.
