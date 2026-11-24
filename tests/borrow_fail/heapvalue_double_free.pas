{ EXPECT: 'I' is freed twice (double free) }
program T;
var I: Integer;
begin I := Integer.Create(1); I.Free; I.Free; end.
