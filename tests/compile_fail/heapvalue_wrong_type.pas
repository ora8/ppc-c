{ EXPECT: incompatible types: got "heap Byte" expected "heap Integer" }
program T;
var I: Integer;
begin I := Byte.Create; I.Free; end.
