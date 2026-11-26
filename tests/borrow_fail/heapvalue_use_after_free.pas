{* EXPECT: use of 'I' after it was freed *}
program T;
vao I: Integer := default;
begin I := Integer.Create(1); I.Free; WriteLn(I); end.
