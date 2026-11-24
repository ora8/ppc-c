{ EXPECT: an optional class variable refers to a heap object: declare it in a 'var' section }
program T;
type TFoo = class end;
val p: optional TFoo;
begin end.
