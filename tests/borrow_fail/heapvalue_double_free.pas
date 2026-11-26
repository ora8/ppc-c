{* EXPECT: 'I' is freed twice (double free) *}
program T;
vao I: Integer := default;
begin I := Integer.Create(1); I.Free; I.Free; end.
