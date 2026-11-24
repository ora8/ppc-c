{ EXPECT: 'nil' does not exist: optional values are empty with 'none' }
program T;
type TFoo = class end;
vao h: optional TFoo := none;
begin h := nil; end.
