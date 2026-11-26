{* EXPECT: was resized by SetLength *}
program T;
val d: array of Integer := (1, 2, 3);
begin
  for var x in d do begin SetLength(d, 0); x := 1; end;
end.
