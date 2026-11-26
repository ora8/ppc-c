{* EXPECT: 'Self' is used (call of 'Show') before the constructor initialized field(s) 'Data' *}
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
type TNode = class Data: TFoo; constructor Create; procedure Show; destructor Destroy; override; end;
constructor TNode.Create; begin inherited Create; Show; Data := TFoo.Create(1); end;
procedure TNode.Show; begin WriteLn(Data.V); end;
destructor TNode.Destroy; begin Data.Free; inherited Destroy; end;
begin end.
