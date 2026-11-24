{ EXPECT: constructor 'TNode.Create' does not initialize non-nilable field 'Data' }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
type TNode = class Data: TFoo; constructor Create; destructor Destroy; override; end;
constructor TNode.Create; begin inherited Create; end;
destructor TNode.Destroy; begin Data.Free; inherited Destroy; end;
begin end.
