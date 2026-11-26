{* EXPECT: identifier not found "I" *}
program W;
begin
  while val I := 0; I < 2 do Inc(I);
  WriteLn(I);
end.
