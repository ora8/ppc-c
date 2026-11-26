{* EXPECT: constructor 'TNode.Create' does not initialize non-nilable field 'Data' on every path *}
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
type TNode = class Data: TFoo; constructor Create(c: Boolean); destructor Destroy; override; end;
constructor TNode.Create(c: Boolean); begin inherited Create; if c then Data := TFoo.Create(1); end;
destructor TNode.Destroy; begin Data.Free; inherited Destroy; end;
begin end.
