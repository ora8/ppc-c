{ EXPECT: array element of type "TFoo" must be declared 'nilable TFoo' }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
var a: array[1..3] of TFoo;
begin end.
