{ EXPECT: a nilable class variable refers to a heap object: declare it in a 'heap' section }
program T;
type TFoo = class end;
var p: nilable TFoo;
begin end.
