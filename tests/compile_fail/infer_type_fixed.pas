{ EXPECT: incompatible types in assignment: got "String" expected "Integer" }
program T;
val I;
begin I := 5; I := 'abc'; end.
