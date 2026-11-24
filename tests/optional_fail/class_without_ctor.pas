{ EXPECT: class 'TNode' has the non-nilable field 'Data' but declares no constructor that initializes it }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
type TNode = class Data: TFoo; end;
begin end.
