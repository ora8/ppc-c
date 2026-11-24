{ EXPECT: 'k' would be a stack object: declare it with a type }
program T; type TFoo = class V: Integer; constructor Create; end; constructor TFoo.Create; begin inherited Create; end;
val k = TFoo.Create;
begin end.
