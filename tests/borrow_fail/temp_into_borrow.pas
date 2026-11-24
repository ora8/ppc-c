{ EXPECT: an owned temporary (Create(...)) cannot be stored in borrowed 'r' }
program T; type TFoo = class V: Integer; end;
var r: TFoo := TFoo.Create;
begin  end.
