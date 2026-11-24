{ EXPECT: "TA" is not a generic type }
program T;
type TA = class end;
vao b := TA<Integer>.Create;
begin end.
