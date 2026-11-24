{ EXPECT: array element of type "TFoo" must be declared 'optional TFoo' }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
val a: array[1..3] of TFoo;
begin end.
