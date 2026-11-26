{* EXPECT: incompatible types: got "heap Byte" expected "heap Integer" *}
program T;
vao I: Integer := default;
begin I := Byte.Create; I.Free; end.
