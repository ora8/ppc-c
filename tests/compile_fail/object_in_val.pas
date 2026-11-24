{ EXPECT: an object variable is declared with 'vao' (owns the object) or 'var' (reference without ownership): 'vao p := ...' }
program T;
type TFoo = class end;
val p: optional TFoo := none;
begin end.
