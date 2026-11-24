{ EXPECT: an owned temporary (Create(...)) cannot be stored in borrowed 'r' }
program T; type TFoo = class V: Integer; end;
val r: borrowed TFoo;
begin r := TFoo.Create; end.
