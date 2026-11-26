{* EXPECT: incompatible types in assignment: got "String" expected "Integer" *}
program T;
val I := 5;
begin I := 'abc'; end.
