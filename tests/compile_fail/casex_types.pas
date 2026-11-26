{* EXPECT: incompatible types in case value: got "String" expected "Integer" *}
program T;
val i: Integer := 1;
begin WriteLn(case i of 1: 10 else 'many' end); end.
