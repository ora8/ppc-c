{ EXPECT: 'default' would be no routine, but "function(): Integer" needs one (give a routine or a lambda, or declare it 'optional') }
program T;
type TF = function: Integer;
val f: TF := default;
begin end.
