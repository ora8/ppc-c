{ EXPECT: the call of 'Show' uses 'G' before an object was assigned to it }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
heap G: TFoo;
procedure Show; begin WriteLn(G.V); end;
begin Show; G := TFoo.Create(1); G.Free; end.
