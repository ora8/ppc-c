{* EXPECT: 'var' names are not possible in a case expression: it only reads the value *}
program T;
type TShape = (Named(Name: String), Empty);
val s: TShape := Empty;
begin WriteLn(case s of Named(var n): n else '' end); end.
