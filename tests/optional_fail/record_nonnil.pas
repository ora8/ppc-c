{ EXPECT: record field of type "TFoo" must be declared 'optional TFoo' }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
type R = record F: TFoo; end;
begin end.
