{* EXPECT: a case expression on "Integer" needs an 'else' (its value must always be defined) *}
program T;
val i: Integer := 1;
begin WriteLn(case i of 1: 'one'; 2: 'two' end); end.
