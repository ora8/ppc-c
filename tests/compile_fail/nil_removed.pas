{ EXPECT: 'nil' does not exist: optional values are empty with 'none' }
program T;
type TFoo = class end;
var h: optional TFoo;
begin h := nil; end.
