{* EXPECT: a variable of the main program cannot be "reference to procedure()" *}
program T;
val g: reference to procedure := procedure begin end;
begin end.
