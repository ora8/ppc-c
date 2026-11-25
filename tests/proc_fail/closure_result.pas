{ EXPECT: the result of a function cannot be "reference to function(): Integer" }
program T;
function Make: reference to function: Integer; begin end;
begin end.
