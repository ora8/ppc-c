{ EXPECT: use of 'I' after it was freed }
program T;
var I: Integer;
begin I := Integer.Create(1); I.Free; WriteLn(I); end.
