{ EXPECT: stack object 's' is used before it is always constructed }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P(c: Boolean); var s: TFoo; begin if c then s.Create(1); WriteLn(s.V); end;
begin P(True); end.
